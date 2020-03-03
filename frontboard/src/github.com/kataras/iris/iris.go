package iris

import (
	// std packages
	"bytes"
	stdContext "context"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"

	// context for the handlers
	"github.com/kataras/iris/v12/context"
	// core packages, required to build the application
	"github.com/kataras/iris/v12/core/errgroup"
	"github.com/kataras/iris/v12/core/host"
	"github.com/kataras/iris/v12/core/netutil"
	"github.com/kataras/iris/v12/core/router"

	// handlerconv conversions
	"github.com/kataras/iris/v12/core/handlerconv"
	// cache conversions
	"github.com/kataras/iris/v12/cache"
	// view
	"github.com/kataras/iris/v12/view"
	// i18n
	"github.com/kataras/iris/v12/i18n"
	// handlers used in `Default` function
	requestLogger "github.com/kataras/iris/v12/middleware/logger"
	"github.com/kataras/iris/v12/middleware/recover"

	"github.com/kataras/golog"
	"gopkg.in/yaml.v3"
)

// Version is the current version number of the Iris Web Framework.
const Version = "12.1.8"

// HTTP status codes as registered with IANA.
// See: http://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml.
// Raw Copy from the future(tip) net/http std package in order to recude the import path of "net/http" for the users.
const (
	StatusContinue             = 100 // RFC 7231, 6.2.1
	StatusSwitchingProtocols   = 101 // RFC 7231, 6.2.2
	StatusProcessing           = 102 // RFC 2518, 10.1
	StatusEarlyHints           = 103 // RFC 8297
	StatusOK                   = 200 // RFC 7231, 6.3.1
	StatusCreated              = 201 // RFC 7231, 6.3.2
	StatusAccepted             = 202 // RFC 7231, 6.3.3
	StatusNonAuthoritativeInfo = 203 // RFC 7231, 6.3.4
	StatusNoContent            = 204 // RFC 7231, 6.3.5
	StatusResetContent         = 205 // RFC 7231, 6.3.6
	StatusPartialContent       = 206 // RFC 7233, 4.1
	StatusMultiStatus          = 207 // RFC 4918, 11.1
	StatusAlreadyReported      = 208 // RFC 5842, 7.1
	StatusIMUsed               = 226 // RFC 3229, 10.4.1

	StatusMultipleChoices  = 300 // RFC 7231, 6.4.1
	StatusMovedPermanently = 301 // RFC 7231, 6.4.2
	StatusFound            = 302 // RFC 7231, 6.4.3
	StatusSeeOther         = 303 // RFC 7231, 6.4.4
	StatusNotModified      = 304 // RFC 7232, 4.1
	StatusUseProxy         = 305 // RFC 7231, 6.4.5

	StatusTemporaryRedirect = 307 // RFC 7231, 6.4.7
	StatusPermanentRedirect = 308 // RFC 7538, 3

	StatusBadRequest                   = 400 // RFC 7231, 6.5.1
	StatusUnauthorized                 = 401 // RFC 7235, 3.1
	StatusPaymentRequired              = 402 // RFC 7231, 6.5.2
	StatusForbidden                    = 403 // RFC 7231, 6.5.3
	StatusNotFound                     = 404 // RFC 7231, 6.5.4
	StatusMethodNotAllowed             = 405 // RFC 7231, 6.5.5
	StatusNotAcceptable                = 406 // RFC 7231, 6.5.6
	StatusProxyAuthRequired            = 407 // RFC 7235, 3.2
	StatusRequestTimeout               = 408 // RFC 7231, 6.5.7
	StatusConflict                     = 409 // RFC 7231, 6.5.8
	StatusGone                         = 410 // RFC 7231, 6.5.9
	StatusLengthRequired               = 411 // RFC 7231, 6.5.10
	StatusPreconditionFailed           = 412 // RFC 7232, 4.2
	StatusRequestEntityTooLarge        = 413 // RFC 7231, 6.5.11
	StatusRequestURITooLong            = 414 // RFC 7231, 6.5.12
	StatusUnsupportedMediaType         = 415 // RFC 7231, 6.5.13
	StatusRequestedRangeNotSatisfiable = 416 // RFC 7233, 4.4
	StatusExpectationFailed            = 417 // RFC 7231, 6.5.14
	StatusTeapot                       = 418 // RFC 7168, 2.3.3
	StatusMisdirectedRequest           = 421 // RFC 7540, 9.1.2
	StatusUnprocessableEntity          = 422 // RFC 4918, 11.2
	StatusLocked                       = 423 // RFC 4918, 11.3
	StatusFailedDependency             = 424 // RFC 4918, 11.4
	StatusTooEarly                     = 425 // RFC 8470, 5.2.
	StatusUpgradeRequired              = 426 // RFC 7231, 6.5.15
	StatusPreconditionRequired         = 428 // RFC 6585, 3
	StatusTooManyRequests              = 429 // RFC 6585, 4
	StatusRequestHeaderFieldsTooLarge  = 431 // RFC 6585, 5
	StatusUnavailableForLegalReasons   = 451 // RFC 7725, 3

	StatusInternalServerError           = 500 // RFC 7231, 6.6.1
	StatusNotImplemented                = 501 // RFC 7231, 6.6.2
	StatusBadGateway                    = 502 // RFC 7231, 6.6.3
	StatusServiceUnavailable            = 503 // RFC 7231, 6.6.4
	StatusGatewayTimeout                = 504 // RFC 7231, 6.6.5
	StatusHTTPVersionNotSupported       = 505 // RFC 7231, 6.6.6
	StatusVariantAlsoNegotiates         = 506 // RFC 2295, 8.1
	StatusInsufficientStorage           = 507 // RFC 4918, 11.5
	StatusLoopDetected                  = 508 // RFC 5842, 7.2
	StatusNotExtended                   = 510 // RFC 2774, 7
	StatusNetworkAuthenticationRequired = 511 // RFC 6585, 6
)

// HTTP Methods copied from `net/http`.
const (
	MethodGet     = "GET"
	MethodPost    = "POST"
	MethodPut     = "PUT"
	MethodDelete  = "DELETE"
	MethodConnect = "CONNECT"
	MethodHead    = "HEAD"
	MethodPatch   = "PATCH"
	MethodOptions = "OPTIONS"
	MethodTrace   = "TRACE"
)

// MethodNone is an iris-specific "virtual" method
// to store the "offline" routes.
const MethodNone = "NONE"

// Application is responsible to manage the state of the application.
// It contains and handles all the necessary parts to create a fast web server.
type Application struct {
	// routing embedded | exposing APIBuilder's and Router's public API.
	*router.APIBuilder
	*router.Router
	ContextPool *context.Pool

	// config contains the configuration fields
	// all fields defaults to something that is working, developers don't have to set it.
	config *Configuration

	// the golog logger instance, defaults to "Info" level messages (all except "Debug")
	logger *golog.Logger

	// I18n contains localization and internationalization support.
	// Use the `Load` or `LoadAssets` to locale language files.
	//
	// See `Context#Tr` method for request-based translations.
	I18n *i18n.I18n

	// view engine
	view view.View
	// used for build
	builded     bool
	defaultMode bool

	mu sync.Mutex
	// Hosts contains a list of all servers (Host Supervisors) that this app is running on.
	//
	// Hosts may be empty only if application ran(`app.Run`) with `iris.Raw` option runner,
	// otherwise it contains a single host (`app.Hosts[0]`).
	//
	// Additional Host Supervisors can be added to that list by calling the `app.NewHost` manually.
	//
	// Hosts field is available after `Run` or `NewHost`.
	Hosts             []*host.Supervisor
	hostConfigurators []host.Configurator
}

// New creates and returns a fresh empty iris *Application instance.
func New() *Application {
	config := DefaultConfiguration()

	app := &Application{
		config:     &config,
		logger:     golog.Default,
		I18n:       i18n.New(),
		APIBuilder: router.NewAPIBuilder(),
		Router:     router.NewRouter(),
	}

	app.ContextPool = context.New(func() context.Context {
		return context.NewContext(app)
	})

	return app
}

// Default returns a new Application instance which on build state registers
// html view engine on "./views" and load locales from "./locales/*/*".
// The return instance recovers on panics and logs the incoming http requests too.
func Default() *Application {
	app := New()
	app.Use(recover.New())
	app.Use(requestLogger.New())
	app.defaultMode = true

	return app
}

// WWW creates and returns a "www." subdomain.
// The difference from `app.Subdomain("www")` or `app.Party("www.")` is that the `app.WWW()` method
// wraps the router so all http(s)://mydomain.com will be redirect to http(s)://www.mydomain.com.
// Other subdomains can be registered using the app: `sub := app.Subdomain("mysubdomain")`,
// child subdomains can be registered using the www := app.WWW(); www.Subdomain("wwwchildSubdomain").
func (app *Application) WWW() router.Party {
	return app.SubdomainRedirect(app, app.Subdomain("www"))
}

// SubdomainRedirect registers a router wrapper which
// redirects(StatusMovedPermanently) a (sub)domain to another subdomain or to the root domain as fast as possible,
// before the router's try to execute route's handler(s).
//
// It receives two arguments, they are the from and to/target locations,
// 'from' can be a wildcard subdomain as well (app.WildcardSubdomain())
// 'to' is not allowed to be a wildcard for obvious reasons,
// 'from' can be the root domain(app) when the 'to' is not the root domain and visa-versa.
//
// Usage:
// www := app.Subdomain("www") <- same as app.Party("www.")
// app.SubdomainRedirect(app, www)
// This will redirect all http(s)://mydomain.com/%anypath% to http(s)://www.mydomain.com/%anypath%.
//
// One or more subdomain redirects can be used to the same app instance.
//
// If you need more information about this implementation then you have to navigate through
// the `core/router#NewSubdomainRedirectWrapper` function instead.
//
// Example: https://github.com/kataras/iris/tree/master/_examples/subdomains/redirect
func (app *Application) SubdomainRedirect(from, to router.Party) router.Party {
	sd := router.NewSubdomainRedirectWrapper(app.ConfigurationReadOnly().GetVHost, from.GetRelPath(), to.GetRelPath())
	app.WrapRouter(sd)
	return to
}

// Configure can called when modifications to the framework instance needed.
// It accepts the framework instance
// and returns an error which if it's not nil it's printed to the logger.
// See configuration.go for more.
//
// Returns itself in order to be used like `app:= New().Configure(...)`
func (app *Application) Configure(configurators ...Configurator) *Application {
	for _, cfg := range configurators {
		if cfg != nil {
			cfg(app)
		}
	}

	return app
}

// ConfigurationReadOnly returns an object which doesn't allow field writing.
func (app *Application) ConfigurationReadOnly() context.ConfigurationReadOnly {
	return app.config
}

// Logger returns the golog logger instance(pointer) that is being used inside the "app".
//
// Available levels:
// - "disable"
// - "fatal"
// - "error"
// - "warn"
// - "info"
// - "debug"
// Usage: app.Logger().SetLevel("error")
// Defaults to "info" level.
//
// Callers can use the application's logger which is
// the same `golog.Default` logger,
// to print custom logs too.
// Usage:
// app.Logger().Error/Errorf("...")
// app.Logger().Warn/Warnf("...")
// app.Logger().Info/Infof("...")
// app.Logger().Debug/Debugf("...")
//
// Setting one or more outputs: app.Logger().SetOutput(io.Writer...)
// Adding one or more outputs : app.Logger().AddOutput(io.Writer...)
//
// Adding custom levels requires import of the `github.com/kataras/golog` package:
//	First we create our level to a golog.Level
//	in order to be used in the Log functions.
//	var SuccessLevel golog.Level = 6
//	Register our level, just three fields.
//	golog.Levels[SuccessLevel] = &golog.LevelMetadata{
//		Name:    "success",
//		RawText: "[SUCC]",
//		// ColorfulText (Green Color[SUCC])
//		ColorfulText: "\x1b[32m[SUCC]\x1b[0m",
//	}
// Usage:
// app.Logger().SetLevel("success")
// app.Logger().Logf(SuccessLevel, "a custom leveled log message")
func (app *Application) Logger() *golog.Logger {
	return app.logger
}

// I18nReadOnly returns the i18n's read-only features.
// See `I18n` method for more.
func (app *Application) I18nReadOnly() context.I18nReadOnly {
	return app.I18n
}

var (
	// HTML view engine.
	// Shortcut of the kataras/iris/view.HTML.
	HTML = view.HTML
	// Django view engine.
	// Shortcut of the kataras/iris/view.Django.
	Django = view.Django
	// Handlebars view engine.
	// Shortcut of the kataras/iris/view.Handlebars.
	Handlebars = view.Handlebars
	// Pug view engine.
	// Shortcut of the kataras/iris/view.Pug.
	Pug = view.Pug
	// Amber view engine.
	// Shortcut of the kataras/iris/view.Amber.
	Amber = view.Amber
	// Jet view engine.
	// Shortcut of the kataras/iris/view.Jet.
	Jet = view.Jet
)

// NoLayout to disable layout for a particular template file
// A shortcut for the `view#NoLayout`.
const NoLayout = view.NoLayout

// RegisterView should be used to register view engines mapping to a root directory
// and the template file(s) extension.
func (app *Application) RegisterView(viewEngine view.Engine) {
	app.view.Register(viewEngine)
}

// View executes and writes the result of a template file to the writer.
//
// First parameter is the writer to write the parsed template.
// Second parameter is the relative, to templates directory, template filename, including extension.
// Third parameter is the layout, can be empty string.
// Forth parameter is the bindable data to the template, can be nil.
//
// Use context.View to render templates to the client instead.
// Returns an error on failure, otherwise nil.
func (app *Application) View(writer io.Writer, filename string, layout string, bindingData interface{}) error {
	if app.view.Len() == 0 {
		err := errors.New("view engine is missing, use `RegisterView`")
		app.Logger().Error(err)
		return err
	}

	err := app.view.ExecuteWriter(writer, filename, layout, bindingData)
	if err != nil {
		app.Logger().Error(err)
	}
	return err
}

var (
	// LimitRequestBodySize is a middleware which sets a request body size limit
	// for all next handlers in the chain.
	//
	// A shortcut for the `context#LimitRequestBodySize`.
	LimitRequestBodySize = context.LimitRequestBodySize
	// NewConditionalHandler returns a single Handler which can be registered
	// as a middleware.
	// Filter is just a type of Handler which returns a boolean.
	// Handlers here should act like middleware, they should contain `ctx.Next` to proceed
	// to the next handler of the chain. Those "handlers" are registered to the per-request context.
	//
	//
	// It checks the "filter" and if passed then
	// it, correctly, executes the "handlers".
	//
	// If passed, this function makes sure that the Context's information
	// about its per-request handler chain based on the new "handlers" is always updated.
	//
	// If not passed, then simply the Next handler(if any) is executed and "handlers" are ignored.
	// Example can be found at: _examples/routing/conditional-chain.
	//
	// A shortcut for the `context#NewConditionalHandler`.
	NewConditionalHandler = context.NewConditionalHandler
	// FileServer returns a Handler which serves files from a specific system, phyisical, directory
	// or an embedded one.
	// The first parameter is the directory, relative to the executable program.
	// The second optional parameter is any optional settings that the caller can use.
	//
	// See `Party#HandleDir` too.
	// Examples can be found at: https://github.com/kataras/iris/tree/master/_examples/file-server
	// A shortcut for the `router.FileServer`.
	FileServer = router.FileServer
	// StripPrefix returns a handler that serves HTTP requests
	// by removing the given prefix from the request URL's Path
	// and invoking the handler h. StripPrefix handles a
	// request for a path that doesn't begin with prefix by
	// replying with an HTTP 404 not found error.
	//
	// Usage:
	// fileserver := iris.FileServer("./static_files", DirOptions {...})
	// h := iris.StripPrefix("/static", fileserver)
	// app.Get("/static/{file:path}", h)
	// app.Head("/static/{file:path}", h)
	StripPrefix = router.StripPrefix
	// Gzip is a middleware which enables writing
	// using gzip compression, if client supports.
	//
	// A shortcut for the `context#Gzip`.
	Gzip = context.Gzip
	// FromStd converts native http.Handler, http.HandlerFunc & func(w, r, next) to context.Handler.
	//
	// Supported form types:
	// 		 .FromStd(h http.Handler)
	// 		 .FromStd(func(w http.ResponseWriter, r *http.Request))
	// 		 .FromStd(func(w http.ResponseWriter, r *http.Request, next http.HandlerFunc))
	//
	// A shortcut for the `handlerconv#FromStd`.
	FromStd = handlerconv.FromStd
	// Cache is a middleware providing server-side cache functionalities
	// to the next handlers, can be used as: `app.Get("/", iris.Cache, aboutHandler)`.
	// It should be used after Static methods.
	// See `iris#Cache304` for an alternative, faster way.
	//
	// Examples can be found at: https://github.com/kataras/iris/tree/master/_examples/#caching
	Cache = cache.Handler
	// NoCache is a middleware which overrides the Cache-Control, Pragma and Expires headers
	// in order to disable the cache during the browser's back and forward feature.
	//
	// A good use of this middleware is on HTML routes; to refresh the page even on "back" and "forward" browser's arrow buttons.
	//
	// See `iris#StaticCache` for the opposite behavior.
	//
	// A shortcut of the `cache#NoCache`
	NoCache = cache.NoCache
	// StaticCache middleware for caching static files by sending the "Cache-Control" and "Expires" headers to the client.
	// It accepts a single input parameter, the "cacheDur", a time.Duration that it's used to calculate the expiration.
	//
	// If "cacheDur" <=0 then it returns the `NoCache` middleware instaed to disable the caching between browser's "back" and "forward" actions.
	//
	// Usage: `app.Use(iris.StaticCache(24 * time.Hour))` or `app.Use(iris.StaticCache(-1))`.
	// A middleware, which is a simple Handler can be called inside another handler as well, example:
	// cacheMiddleware := iris.StaticCache(...)
	// func(ctx iris.Context){
	//  cacheMiddleware(ctx)
	//  [...]
	// }
	//
	// A shortcut of the `cache#StaticCache`
	StaticCache = cache.StaticCache
	// Cache304 sends a `StatusNotModified` (304) whenever
	// the "If-Modified-Since" request header (time) is before the
	// time.Now() + expiresEvery (always compared to their UTC values).
	// Use this, which is a shortcut of the, `chache#Cache304` instead of the "github.com/kataras/iris/v12/cache" or iris.Cache
	// for better performance.
	// Clients that are compatible with the http RCF (all browsers are and tools like postman)
	// will handle the caching.
	// The only disadvantage of using that instead of server-side caching
	// is that this method will send a 304 status code instead of 200,
	// So, if you use it side by side with other micro services
	// you have to check for that status code as well for a valid response.
	//
	// Developers are free to extend this method's behavior
	// by watching system directories changes manually and use of the `ctx.WriteWithExpiration`
	// with a "modtime" based on the file modified date,
	// similar to the `HandleDir`(which sends status OK(200) and browser disk caching instead of 304).
	//
	// A shortcut of the `cache#Cache304`.
	Cache304 = cache.Cache304
	// CookiePath is a `CookieOption`.
	// Use it to change the cookie's Path field.
	//
	// A shortcut for the `context#CookiePath`.
	CookiePath = context.CookiePath
	// CookieCleanPath is a `CookieOption`.
	// Use it to clear the cookie's Path field, exactly the same as `CookiePath("")`.
	//
	// A shortcut for the `context#CookieCleanPath`.
	CookieCleanPath = context.CookieCleanPath
	// CookieExpires is a `CookieOption`.
	// Use it to change the cookie's Expires and MaxAge fields by passing the lifetime of the cookie.
	//
	// A shortcut for the `context#CookieExpires`.
	CookieExpires = context.CookieExpires
	// CookieHTTPOnly is a `CookieOption`.
	// Use it to set the cookie's HttpOnly field to false or true.
	// HttpOnly field defaults to true for `RemoveCookie` and `SetCookieKV`.
	//
	// A shortcut for the `context#CookieHTTPOnly`.
	CookieHTTPOnly = context.CookieHTTPOnly
	// CookieEncode is a `CookieOption`.
	// Provides encoding functionality when adding a cookie.
	// Accepts a `context#CookieEncoder` and sets the cookie's value to the encoded value.
	// Users of that is the `context#SetCookie` and `context#SetCookieKV`.
	//
	// Example: https://github.com/kataras/iris/tree/master/_examples/cookies/securecookie
	//
	// A shortcut for the `context#CookieEncode`.
	CookieEncode = context.CookieEncode
	// CookieDecode is a `CookieOption`.
	// Provides decoding functionality when retrieving a cookie.
	// Accepts a `context#CookieDecoder` and sets the cookie's value to the decoded value before return by the `GetCookie`.
	// User of that is the `context#GetCookie`.
	//
	// Example: https://github.com/kataras/iris/tree/master/_examples/cookies/securecookie
	//
	// A shortcut for the `context#CookieDecode`.
	CookieDecode = context.CookieDecode
	// IsErrPath can be used at `context#ReadForm`.
	// It reports whether the incoming error is type of `formbinder.ErrPath`,
	// which can be ignored when server allows unknown post values to be sent by the client.
	//
	// A shortcut for the `context#IsErrPath`.
	IsErrPath = context.IsErrPath
	// NewProblem returns a new Problem.
	// Head over to the `Problem` type godoc for more.
	//
	// A shortcut for the `context#NewProblem`.
	NewProblem = context.NewProblem
	// XMLMap wraps a map[string]interface{} to compatible xml marshaler,
	// in order to be able to render maps as XML on the `Context.XML` method.
	//
	// Example: `Context.XML(XMLMap("Root", map[string]interface{}{...})`.
	//
	// A shortcut for the `context#XMLMap`.
	XMLMap = context.XMLMap
)

// Constants for input argument at `router.RouteRegisterRule`.
// See `Party#SetRegisterRule`.
const (
	// RouteOverride an existing route with the new one, the default rule.
	RouteOverride = router.RouteOverride
	// RouteSkip registering a new route twice.
	RouteSkip = router.RouteSkip
	// RouteError log when a route already exists, shown after the `Build` state,
	// server never starts.
	RouteError = router.RouteError
)

// Contains the enum values of the `Context.GetReferrer()` method,
// shortcuts of the context subpackage.
const (
	ReferrerInvalid  = context.ReferrerInvalid
	ReferrerIndirect = context.ReferrerIndirect
	ReferrerDirect   = context.ReferrerDirect
	ReferrerEmail    = context.ReferrerEmail
	ReferrerSearch   = context.ReferrerSearch
	ReferrerSocial   = context.ReferrerSocial

	ReferrerNotGoogleSearch     = context.ReferrerNotGoogleSearch
	ReferrerGoogleOrganicSearch = context.ReferrerGoogleOrganicSearch
	ReferrerGoogleAdwords       = context.ReferrerGoogleAdwords
)

// ConfigureHost accepts one or more `host#Configuration`, these configurators functions
// can access the host created by `app.Run`,
// they're being executed when application is ready to being served to the public.
//
// It's an alternative way to interact with a host that is automatically created by
// `app.Run`.
//
// These "configurators" can work side-by-side with the `iris#Addr, iris#Server, iris#TLS, iris#AutoTLS, iris#Listener`
// final arguments("hostConfigs") too.
//
// Note that these application's host "configurators" will be shared with the rest of
// the hosts that this app will may create (using `app.NewHost`), meaning that
// `app.NewHost` will execute these "configurators" everytime that is being called as well.
//
// These "configurators" should be registered before the `app.Run` or `host.Serve/Listen` functions.
func (app *Application) ConfigureHost(configurators ...host.Configurator) *Application {
	app.mu.Lock()
	app.hostConfigurators = append(app.hostConfigurators, configurators...)
	app.mu.Unlock()
	return app
}

// NewHost accepts a standard *http.Server object,
// completes the necessary missing parts of that "srv"
// and returns a new, ready-to-use, host (supervisor).
func (app *Application) NewHost(srv *http.Server) *host.Supervisor {
	app.mu.Lock()
	defer app.mu.Unlock()

	// set the server's handler to the framework's router
	if srv.Handler == nil {
		srv.Handler = app.Router
	}

	// check if different ErrorLog provided, if not bind it with the framework's logger
	if srv.ErrorLog == nil {
		srv.ErrorLog = log.New(app.logger.Printer.Output, "[HTTP Server] ", 0)
	}

	if addr := srv.Addr; addr == "" {
		addr = ":8080"
		if len(app.Hosts) > 0 {
			if v := app.Hosts[0].Server.Addr; v != "" {
				addr = v
			}
		}

		srv.Addr = addr
	}

	app.logger.Debugf("Host: addr is %s", srv.Addr)

	// create the new host supervisor
	// bind the constructed server and return it
	su := host.New(srv)

	if app.config.vhost == "" { // vhost now is useful for router subdomain on wildcard subdomains,
		// in order to correct decide what to do on:
		// mydomain.com -> invalid
		// localhost -> invalid
		// sub.mydomain.com -> valid
		// sub.localhost -> valid
		// we need the host (without port if 80 or 443) in order to validate these, so:
		app.config.vhost = netutil.ResolveVHost(srv.Addr)
	}

	app.logger.Debugf("Host: virtual host is %s", app.config.vhost)

	// the below schedules some tasks that will run among the server

	if !app.config.DisableStartupLog {
		// show the available info to exit from app.
		su.RegisterOnServe(host.WriteStartupLogOnServe(app.logger.Printer.Output)) // app.logger.Writer -> Info
		app.logger.Debugf("Host: register startup notifier")
	}

	if !app.config.DisableInterruptHandler {
		// when CTRL+C/CMD+C pressed.
		shutdownTimeout := 5 * time.Second
		host.RegisterOnInterrupt(host.ShutdownOnInterrupt(su, shutdownTimeout))
		app.logger.Debugf("Host: register server shutdown on interrupt(CTRL+C/CMD+C)")
	}

	su.IgnoredErrors = append(su.IgnoredErrors, app.config.IgnoreServerErrors...)
	if len(su.IgnoredErrors) > 0 {
		app.logger.Debugf("Host: server will ignore the following errors: %s", su.IgnoredErrors)
	}

	su.Configure(app.hostConfigurators...)

	app.Hosts = append(app.Hosts, su)

	return su
}

// RegisterOnInterrupt registers a global function to call when CTRL+C/CMD+C pressed or a unix kill command received.
//
// A shortcut for the `host#RegisterOnInterrupt`.
var RegisterOnInterrupt = host.RegisterOnInterrupt

// Shutdown gracefully terminates all the application's server hosts and any tunnels.
// Returns an error on the first failure, otherwise nil.
func (app *Application) Shutdown(ctx stdContext.Context) error {
	for i, su := range app.Hosts {
		app.logger.Debugf("Host[%d]: Shutdown now", i)
		if err := su.Shutdown(ctx); err != nil {
			app.logger.Debugf("Host[%d]: Error while trying to shutdown", i)
			return err
		}
	}

	for _, t := range app.config.Tunneling.Tunnels {
		if t.Name == "" {
			continue
		}

		if err := app.config.Tunneling.stopTunnel(t); err != nil {
			return err
		}
	}

	return nil
}

// Build sets up, once, the framework.
// It builds the default router with its default macros
// and the template functions that are very-closed to iris.
//
// If error occurred while building the Application, the returns type of error will be an *errgroup.Group
// which let the callers to inspect the errors and cause, usage:
//
// import "github.com/kataras/iris/v12/core/errgroup"
//
// errgroup.Walk(app.Build(), func(typ interface{}, err error) {
// 	app.Logger().Errorf("%s: %s", typ, err)
// })
func (app *Application) Build() error {
	rp := errgroup.New("Application Builder")

	if !app.builded {
		app.builded = true
		rp.Err(app.APIBuilder.GetReporter())

		if app.defaultMode { // the app.I18n and app.View will be not available until Build.
			if !app.I18n.Loaded() {
				for _, s := range []string{"./locales/*/*", "./locales/*", "./translations"} {
					if _, err := os.Stat(s); os.IsNotExist(err) {
						continue
					}

					if err := app.I18n.Load(s); err != nil {
						continue
					}

					app.I18n.SetDefault("en-US")
					break
				}
			}

			if app.view.Len() == 0 {
				for _, s := range []string{"./views", "./templates", "./web/views"} {
					if _, err := os.Stat(s); os.IsNotExist(err) {
						continue
					}

					app.RegisterView(HTML(s, ".html"))
					break
				}
			}
		}

		if app.I18n.Loaded() {
			// {{ tr "lang" "key" arg1 arg2 }}
			app.view.AddFunc("tr", app.I18n.Tr)
			app.WrapRouter(app.I18n.Wrapper())
		}

		if !app.Router.Downgraded() {
			// router

			if err := app.tryInjectLiveReload(); err != nil {
				rp.Errf("LiveReload: init: failed: %v", err)
			}

			// create the request handler, the default routing handler
			routerHandler := router.NewDefaultHandler()
			err := app.Router.BuildRouter(app.ContextPool, routerHandler, app.APIBuilder, false)
			if err != nil {
				rp.Err(err)
			}
			// re-build of the router from outside can be done with
			// app.RefreshRouter()
		}

		if app.view.Len() > 0 {
			app.logger.Debugf("Application: %d registered view engine(s)", app.view.Len())
			// view engine
			// here is where we declare the closed-relative framework functions.
			// Each engine has their defaults, i.e yield,render,render_r,partial, params...
			rv := router.NewRoutePathReverser(app.APIBuilder)
			app.view.AddFunc("urlpath", rv.Path)
			// app.view.AddFunc("url", rv.URL)
			if err := app.view.Load(); err != nil {
				rp.Group("View Builder").Err(err)
			}
		}
	}

	return errgroup.Check(rp)
}

// Runner is just an interface which accepts the framework instance
// and returns an error.
//
// It can be used to register a custom runner with `Run` in order
// to set the framework's server listen action.
//
// Currently `Runner` is being used to declare the builtin server listeners.
//
// See `Run` for more.
type Runner func(*Application) error

// Listener can be used as an argument for the `Run` method.
// It can start a server with a custom net.Listener via server's `Serve`.
//
// Second argument is optional, it accepts one or more
// `func(*host.Configurator)` that are being executed
// on that specific host that this function will create to start the server.
// Via host configurators you can configure the back-end host supervisor,
// i.e to add events for shutdown, serve or error.
// An example of this use case can be found at:
// https://github.com/kataras/iris/blob/master/_examples/http-listening/notify-on-shutdown/main.go
// Look at the `ConfigureHost` too.
//
// See `Run` for more.
func Listener(l net.Listener, hostConfigs ...host.Configurator) Runner {
	return func(app *Application) error {
		app.config.vhost = netutil.ResolveVHost(l.Addr().String())
		return app.NewHost(&http.Server{Addr: l.Addr().String()}).
			Configure(hostConfigs...).
			Serve(l)
	}
}

// Server can be used as an argument for the `Run` method.
// It can start a server with a *http.Server.
//
// Second argument is optional, it accepts one or more
// `func(*host.Configurator)` that are being executed
// on that specific host that this function will create to start the server.
// Via host configurators you can configure the back-end host supervisor,
// i.e to add events for shutdown, serve or error.
// An example of this use case can be found at:
// https://github.com/kataras/iris/blob/master/_examples/http-listening/notify-on-shutdown/main.go
// Look at the `ConfigureHost` too.
//
// See `Run` for more.
func Server(srv *http.Server, hostConfigs ...host.Configurator) Runner {
	return func(app *Application) error {
		return app.NewHost(srv).
			Configure(hostConfigs...).
			ListenAndServe()
	}
}

// Addr can be used as an argument for the `Run` method.
// It accepts a host address which is used to build a server
// and a listener which listens on that host and port.
//
// Addr should have the form of [host]:port, i.e localhost:8080 or :8080.
//
// Second argument is optional, it accepts one or more
// `func(*host.Configurator)` that are being executed
// on that specific host that this function will create to start the server.
// Via host configurators you can configure the back-end host supervisor,
// i.e to add events for shutdown, serve or error.
// An example of this use case can be found at:
// https://github.com/kataras/iris/blob/master/_examples/http-listening/notify-on-shutdown/main.go
// Look at the `ConfigureHost` too.
//
// See `Run` for more.
func Addr(addr string, hostConfigs ...host.Configurator) Runner {
	return func(app *Application) error {
		return app.NewHost(&http.Server{Addr: addr}).
			Configure(hostConfigs...).
			ListenAndServe()
	}
}

// TLS can be used as an argument for the `Run` method.
// It will start the Application's secure server.
//
// Use it like you used to use the http.ListenAndServeTLS function.
//
// Addr should have the form of [host]:port, i.e localhost:443 or :443.
// CertFile & KeyFile should be filenames with their extensions.
//
// Second argument is optional, it accepts one or more
// `func(*host.Configurator)` that are being executed
// on that specific host that this function will create to start the server.
// Via host configurators you can configure the back-end host supervisor,
// i.e to add events for shutdown, serve or error.
// An example of this use case can be found at:
// https://github.com/kataras/iris/blob/master/_examples/http-listening/notify-on-shutdown/main.go
// Look at the `ConfigureHost` too.
//
// See `Run` for more.
func TLS(addr string, certFile, keyFile string, hostConfigs ...host.Configurator) Runner {
	return func(app *Application) error {
		return app.NewHost(&http.Server{Addr: addr}).
			Configure(hostConfigs...).
			ListenAndServeTLS(certFile, keyFile)
	}
}

// AutoTLS can be used as an argument for the `Run` method.
// It will start the Application's secure server using
// certifications created on the fly by the "autocert" golang/x package,
// so localhost may not be working, use it at "production" machine.
//
// Addr should have the form of [host]:port, i.e mydomain.com:443.
//
// The whitelisted domains are separated by whitespace in "domain" argument,
// i.e "iris-go.com", can be different than "addr".
// If empty, all hosts are currently allowed. This is not recommended,
// as it opens a potential attack where clients connect to a server
// by IP address and pretend to be asking for an incorrect host name.
// Manager will attempt to obtain a certificate for that host, incorrectly,
// eventually reaching the CA's rate limit for certificate requests
// and making it impossible to obtain actual certificates.
//
// For an "e-mail" use a non-public one, letsencrypt needs that for your own security.
//
// Note: `AutoTLS` will start a new server for you
// which will redirect all http versions to their https, including subdomains as well.
//
// Last argument is optional, it accepts one or more
// `func(*host.Configurator)` that are being executed
// on that specific host that this function will create to start the server.
// Via host configurators you can configure the back-end host supervisor,
// i.e to add events for shutdown, serve or error.
// An example of this use case can be found at:
// https://github.com/kataras/iris/blob/master/_examples/http-listening/notify-on-shutdown/main.go
// Look at the `ConfigureHost` too.
//
// Usage:
// app.Run(iris.AutoTLS("iris-go.com:443", "iris-go.com www.iris-go.com", "mail@example.com"))
//
// See `Run` and `core/host/Supervisor#ListenAndServeAutoTLS` for more.
func AutoTLS(
	addr string,
	domain string, email string,
	hostConfigs ...host.Configurator) Runner {
	return func(app *Application) error {
		return app.NewHost(&http.Server{Addr: addr}).
			Configure(hostConfigs...).
			ListenAndServeAutoTLS(domain, email, "letscache")
	}
}

// Raw can be used as an argument for the `Run` method.
// It accepts any (listen) function that returns an error,
// this function should be block and return an error
// only when the server exited or a fatal error caused.
//
// With this option you're not limited to the servers
// that iris can run by-default.
//
// See `Run` for more.
func Raw(f func() error) Runner {
	return func(app *Application) error {
		app.logger.Debugf("HTTP Server will start from unknown, external function")
		return f()
	}
}

// ErrServerClosed is returned by the Server's Serve, ServeTLS, ListenAndServe,
// and ListenAndServeTLS methods after a call to Shutdown or Close.
//
// A shortcut for the `http#ErrServerClosed`.
var ErrServerClosed = http.ErrServerClosed

// Listen builds the application and starts the server
// on the TCP network address "host:port" which
// handles requests on incoming connections.
//
// Listen always returns a non-nil error.
// Ignore specific errors by using an `iris.WithoutServerError(iris.ErrServerClosed)`
// as a second input argument.
//
// Listen is a shortcut of `app.Run(iris.Addr(hostPort, withOrWithout...))`.
// See `Run` for details.
func (app *Application) Listen(hostPort string, withOrWithout ...Configurator) error {
	return app.Run(Addr(hostPort), withOrWithout...)
}

// Run builds the framework and starts the desired `Runner` with or without configuration edits.
//
// Run should be called only once per Application instance, it blocks like http.Server.
//
// If more than one server needed to run on the same iris instance
// then create a new host and run it manually by `go NewHost(*http.Server).Serve/ListenAndServe` etc...
// or use an already created host:
// h := NewHost(*http.Server)
// Run(Raw(h.ListenAndServe), WithCharset("UTF-8"), WithRemoteAddrHeader("CF-Connecting-IP"))
//
// The Application can go online with any type of server or iris's host with the help of
// the following runners:
// `Listener`, `Server`, `Addr`, `TLS`, `AutoTLS` and `Raw`.
func (app *Application) Run(serve Runner, withOrWithout ...Configurator) error {
	// first Build because it doesn't need anything from configuration,
	// this gives the user the chance to modify the router inside a configurator as well.
	if err := app.Build(); err != nil {
		app.logger.Error(err)
		return err
	}

	app.Configure(withOrWithout...)
	app.tryStartTunneling()

	app.logger.Debugf("Application: running using %d host(s)", len(app.Hosts)+1)

	// this will block until an error(unless supervisor's DeferFlow called from a Task).
	err := serve(app)
	if err != nil {
		app.Logger().Error(err)
	}

	return err
}

// tryInjectLiveReload tries to check if this application
// runs under https://github.com/kataras/iris-cli and if so
// then it checks if the livereload is enabled and then injects
// the watch listener (js script) on every HTML response.
// It has a slight performance cost but
// this (iris-cli with watch and livereload enabled)
// is meant to be used only in development mode.
// It does a full reload at the moment and if the port changed
// at runtime it will fire 404 instead of redirecting to the correct port (that's a TODO).
//
// tryInjectLiveReload runs right before Build -> BuildRouter.
func (app *Application) tryInjectLiveReload() error {
	conf := struct {
		Running    bool `yaml:"Running,omitempty"`
		LiveReload struct {
			Disable bool `yaml:"Disable"`
			Port    int  `yaml:"Port"`
		} `yaml:"LiveReload"`
	}{}
	// defaults to disabled here.
	conf.LiveReload.Disable = true

	wd, err := os.Getwd()
	if err != nil {
		return err
	}

	for _, path := range []string{".iris.yml" /*, "../.iris.yml", "../../.iris.yml" */} {
		path = filepath.Join(wd, path)

		if _, err := os.Stat(path); err == nil {
			inFile, err := os.OpenFile(path, os.O_RDONLY, 0644)
			if err != nil {
				return err
			}

			dec := yaml.NewDecoder(inFile)
			err = dec.Decode(&conf)
			inFile.Close()
			if err != nil {
				return err
			}

			break
		}
	}

	if !conf.Running || conf.LiveReload.Disable {
		return nil
	}

	scriptReloadJS := []byte(fmt.Sprintf(`<script>(function () {
    const scheme = document.location.protocol == "https:" ? "wss" : "ws";
    const endpoint = scheme + "://" + document.location.hostname + ":%d/livereload";

    w = new WebSocket(endpoint);
    w.onopen = function () {
        console.info("LiveReload: initialization");
    };
    w.onclose = function () {
        console.info("LiveReload: terminated");
    };
    w.onmessage = function (message) {
        // NOTE: full-reload, at least for the moment. Also if backend changed its port then we will get 404 here. 
        window.location.reload();
    };
}());</script>`, conf.LiveReload.Port))

	bodyCloseTag := []byte("</body>")

	app.WrapRouter(func(w http.ResponseWriter, r *http.Request, _ http.HandlerFunc) {
		ctx := app.ContextPool.Acquire(w, r)
		rec := ctx.Recorder() // Record everything and write all in once at the Context release.
		app.ServeHTTPC(ctx)   // We directly call request handler with Context.

		if strings.HasPrefix(ctx.GetContentType(), "text/html") {
			// delete(rec.Header(), context.ContentLengthHeaderKey)

			body := rec.Body()

			if idx := bytes.LastIndex(body, bodyCloseTag); idx > 0 {
				// add the script right before last </body>.
				body = append(body[:idx], bytes.Replace(body[idx:], bodyCloseTag, append(scriptReloadJS, bodyCloseTag...), 1)...)
				rec.SetBody(body)
			} else {
				// Just append it.
				rec.Write(scriptReloadJS)
			}

			if _, has := rec.Header()[context.ContentLengthHeaderKey]; has {
				rec.Header().Set(context.ContentLengthHeaderKey, fmt.Sprintf("%d", len(rec.Body())))
			}
		}

		app.ContextPool.Release(ctx)
	})

	return nil
}

// https://ngrok.com/docs
func (app *Application) tryStartTunneling() {
	if !app.config.Tunneling.isEnabled() {
		return
	}

	app.ConfigureHost(func(su *host.Supervisor) {
		su.RegisterOnServe(func(h host.TaskHost) {
			tc := app.config.Tunneling
			if tc.WebInterface == "" {
				tc.WebInterface = "http://127.0.0.1:4040"
			}

			for tunnIdx, t := range tc.Tunnels {
				if t.Name == "" {
					t.Name = fmt.Sprintf("iris-app-%d-%s", tunnIdx+1, time.Now().Format(app.config.TimeFormat))
				}

				if t.Addr == "" {
					t.Addr = su.Server.Addr
				}

				var publicAddr string
				err := tc.startTunnel(t, &publicAddr)
				if err != nil {
					app.Logger().Errorf("Host: tunneling error: %v", err)
					return
				}

				// to make subdomains resolution still based on this new remote, public addresses.
				app.config.vhost = publicAddr[strings.Index(publicAddr, "://")+3:]

				directLog := []byte(fmt.Sprintf("⬝ Public Address: %s\n", publicAddr))
				app.Logger().Printer.Output.Write(directLog)
			}
		})
	})
}
