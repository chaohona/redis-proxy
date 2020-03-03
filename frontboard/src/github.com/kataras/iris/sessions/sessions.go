package sessions

import (
	"net/http"
	"time"

	"github.com/kataras/iris/v12/context"
)

// A Sessions manager should be responsible to Start a sesion, based
// on a Context, which should return
// a compatible Session interface, type. If the external session manager
// doesn't qualifies, then the user should code the rest of the functions with empty implementation.
//
// Sessions should be responsible to Destroy a session based
// on the Context.
type Sessions struct {
	config   Config
	provider *provider
}

// New returns a new fast, feature-rich sessions manager
// it can be adapted to an iris station
func New(cfg Config) *Sessions {
	return &Sessions{
		config:   cfg.Validate(),
		provider: newProvider(),
	}
}

// UseDatabase adds a session database to the manager's provider,
// a session db doesn't have write access
func (s *Sessions) UseDatabase(db Database) {
	s.provider.RegisterDatabase(db)
}

// updateCookie gains the ability of updating the session browser cookie to any method which wants to update it
func (s *Sessions) updateCookie(ctx context.Context, sid string, expires time.Duration, options ...context.CookieOption) {
	cookie := &http.Cookie{}

	// The RFC makes no mention of encoding url value, so here I think to encode both sessionid key and the value using the safe(to put and to use as cookie) url-encoding
	cookie.Name = s.config.Cookie

	cookie.Value = sid
	cookie.Path = "/"
	cookie.Domain = formatCookieDomain(ctx, s.config.DisableSubdomainPersistence)
	cookie.HttpOnly = true
	if !s.config.DisableSubdomainPersistence {
		cookie.SameSite = http.SameSiteLaxMode // allow subdomain sharing.
	}

	// MaxAge=0 means no 'Max-Age' attribute specified.
	// MaxAge<0 means delete cookie now, equivalently 'Max-Age: 0'
	// MaxAge>0 means Max-Age attribute present and given in seconds
	if expires >= 0 {
		if expires == 0 { // unlimited life
			cookie.Expires = CookieExpireUnlimited
		} else { // > 0
			cookie.Expires = time.Now().Add(expires)
		}
		cookie.MaxAge = int(time.Until(cookie.Expires).Seconds())
	}

	// set the cookie to secure if this is a tls wrapped request
	// and the configuration allows it.
	if ctx.Request().TLS != nil && s.config.CookieSecureTLS {
		cookie.Secure = true
	}

	// encode the session id cookie client value right before send it.
	cookie.Value = s.encodeCookieValue(cookie.Value)

	for _, opt := range options {
		opt(cookie)
	}

	AddCookie(ctx, cookie, s.config.AllowReclaim)
}

// Start creates or retrieves an existing session for the particular request.
func (s *Sessions) Start(ctx context.Context, cookieOptions ...context.CookieOption) *Session {
	cookieValue := s.decodeCookieValue(GetCookie(ctx, s.config.Cookie))

	if cookieValue == "" { // cookie doesn't exist, let's generate a session and set a cookie.
		sid := s.config.SessionIDGenerator(ctx)

		sess := s.provider.Init(sid, s.config.Expires)
		sess.isNew = s.provider.db.Len(sid) == 0

		s.updateCookie(ctx, sid, s.config.Expires, cookieOptions...)

		return sess
	}

	return s.provider.Read(cookieValue, s.config.Expires)
}

const contextSessionKey = "_iris_session"

// Handler returns a sessions middleware to register on application routes.
func (s *Sessions) Handler(cookieOptions ...context.CookieOption) context.Handler {
	return func(ctx context.Context) {
		session := s.Start(ctx, cookieOptions...)
		ctx.Values().Set(contextSessionKey, session)
		ctx.Next()
	}
}

// Get returns a *Session from the same request life cycle,
// can be used inside a chain of handlers of a route.
//
// The `Sessions.Start` should be called previously,
// e.g. register the `Sessions.Handler` as middleware.
// Then call `Get` package-level function as many times as you want.
// The `Sessions.Start` can be called more than one time in the same request life cycle as well.
func Get(ctx context.Context) *Session {
	if v := ctx.Values().Get(contextSessionKey); v != nil {
		if sess, ok := v.(*Session); ok {
			return sess
		}
	}

	return nil
}

// StartWithPath same as `Start` but it explicitly accepts the cookie path option.
func (s *Sessions) StartWithPath(ctx context.Context, path string) *Session {
	return s.Start(ctx, context.CookiePath(path))
}

// ShiftExpiration move the expire date of a session to a new date
// by using session default timeout configuration.
// It will return `ErrNotImplemented` if a database is used and it does not support this feature, yet.
func (s *Sessions) ShiftExpiration(ctx context.Context, cookieOptions ...context.CookieOption) error {
	return s.UpdateExpiration(ctx, s.config.Expires, cookieOptions...)
}

// UpdateExpiration change expire date of a session to a new date
// by using timeout value passed by `expires` receiver.
// It will return `ErrNotFound` when trying to update expiration on a non-existence or not valid session entry.
// It will return `ErrNotImplemented` if a database is used and it does not support this feature, yet.
func (s *Sessions) UpdateExpiration(ctx context.Context, expires time.Duration, cookieOptions ...context.CookieOption) error {
	cookieValue := s.decodeCookieValue(GetCookie(ctx, s.config.Cookie))
	if cookieValue == "" {
		return ErrNotFound
	}

	// we should also allow it to expire when the browser closed
	err := s.provider.UpdateExpiration(cookieValue, expires)
	if err == nil || expires == -1 {
		s.updateCookie(ctx, cookieValue, expires, cookieOptions...)
	}

	return err
}

// DestroyListener is the form of a destroy listener.
// Look `OnDestroy` for more.
type DestroyListener func(sid string)

// OnDestroy registers one or more destroy listeners.
// A destroy listener is fired when a session has been removed entirely from the server (the entry) and client-side (the cookie).
// Note that if a destroy listener is blocking, then the session manager will delay respectfully,
// use a goroutine inside the listener to avoid that behavior.
func (s *Sessions) OnDestroy(listeners ...DestroyListener) {
	for _, ln := range listeners {
		s.provider.registerDestroyListener(ln)
	}
}

// Destroy remove the session data and remove the associated cookie.
func (s *Sessions) Destroy(ctx context.Context) {
	cookieValue := GetCookie(ctx, s.config.Cookie)
	// decode the client's cookie value in order to find the server's session id
	// to destroy the session data.
	cookieValue = s.decodeCookieValue(cookieValue)
	if cookieValue == "" { // nothing to destroy
		return
	}
	RemoveCookie(ctx, s.config)

	s.provider.Destroy(cookieValue)
}

// DestroyByID removes the session entry
// from the server-side memory (and database if registered).
// Client's session cookie will still exist but it will be reseted on the next request.
//
// It's safe to use it even if you are not sure if a session with that id exists.
//
// Note: the sid should be the original one (i.e: fetched by a store )
// it's not decoded.
func (s *Sessions) DestroyByID(sid string) {
	s.provider.Destroy(sid)
}

// DestroyAll removes all sessions
// from the server-side memory (and database if registered).
// Client's session cookie will still exist but it will be reseted on the next request.
func (s *Sessions) DestroyAll() {
	s.provider.DestroyAll()
}

// let's keep these funcs simple, we can do it with two lines but we may add more things in the future.
func (s *Sessions) decodeCookieValue(cookieValue string) string {
	if cookieValue == "" {
		return ""
	}

	var cookieValueDecoded string

	if decode := s.config.Decode; decode != nil {
		err := decode(s.config.Cookie, cookieValue, &cookieValueDecoded)
		if err == nil {
			cookieValue = cookieValueDecoded
		} else {
			cookieValue = ""
		}
	}

	return cookieValue
}

func (s *Sessions) encodeCookieValue(cookieValue string) string {
	if encode := s.config.Encode; encode != nil {
		newVal, err := encode(s.config.Cookie, cookieValue)
		if err == nil {
			cookieValue = newVal
		} else {
			cookieValue = ""
		}
	}

	return cookieValue
}
