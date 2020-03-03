package mvc_test

import (
	"testing"

	"github.com/kataras/iris/v12"
	"github.com/kataras/iris/v12/context"
	"github.com/kataras/iris/v12/httptest"

	. "github.com/kataras/iris/v12/mvc"
)

// service
type (
	// these TestService and TestServiceImpl could be in lowercase, unexported
	// but the `Say` method should be exported however we have those exported
	// because of the controller handler test.
	TestService interface {
		Say(string) string
	}
	TestServiceImpl struct {
		prefix string
	}
)

func (s *TestServiceImpl) Say(message string) string {
	return s.prefix + " " + message
}

type testControllerHandle struct {
	Ctx     context.Context
	Service TestService

	reqField string
}

func (c *testControllerHandle) BeforeActivation(b BeforeActivation) {
	b.Handle("GET", "/histatic", "HiStatic")
	b.Handle("GET", "/hiservice", "HiService")
	b.Handle("GET", "/hiservice/{ps:string}", "HiServiceBy")
	b.Handle("GET", "/hiparam/{ps:string}", "HiParamBy")
	b.Handle("GET", "/hiparamempyinput/{ps:string}", "HiParamEmptyInputBy")
	b.HandleMany("GET", "/custom/{ps:string} /custom2/{ps:string}", "CustomWithParameter")
	// if dynamic path exist
	// then longest path should be registered first
	// and the controller's method if wants to add path parameters
	// dependency injection then they should accept the longest path parameters.
	// See `testControllerHandle.CustomWithParameters`.
	b.HandleMany("GET", "/custom3/{ps:string}/{pssecond:string} /custom3/{ps:string}", "CustomWithParameters")
}

// test `GetRoute` for custom routes.
func (c *testControllerHandle) AfterActivation(a AfterActivation) {
	// change automatic parser's route change name.
	rget := a.GetRoute("Get")
	if rget == nil {
		panic("route from function name: 'Get' doesn't exist on `AfterActivation`")
	}
	rget.Name = "index_route"

	// change a custom route's name.
	r := a.GetRoute("HiStatic")
	if r == nil {
		panic("route from function name: HiStatic doesn't exist on `AfterActivation`")
	}
	// change the name here, and test if name changed in the handler.
	r.Name = "hi_static_route"
}

func (c *testControllerHandle) BeginRequest(ctx iris.Context) {
	c.reqField = ctx.URLParam("reqfield")
}

func (c *testControllerHandle) EndRequest(ctx iris.Context) {}

func (c *testControllerHandle) Get() string {
	if c.Ctx.GetCurrentRoute().Name() != "index_route" {
		return "Get's route's name didn't change on AfterActivation"
	}
	return "index"
}

func (c *testControllerHandle) HiStatic() string {
	if c.Ctx.GetCurrentRoute().Name() != "hi_static_route" {
		return "HiStatic's route's name didn't change on AfterActivation"
	}

	return c.reqField
}

func (c *testControllerHandle) HiService() string {
	return c.Service.Say("hi")
}

func (c *testControllerHandle) HiServiceBy(v string) string {
	return c.Service.Say("hi with param: " + v)
}

func (c *testControllerHandle) HiParamBy(v string) string {
	return v
}

func (c *testControllerHandle) HiParamEmptyInputBy() string {
	return "empty in but served with ctx.Params.Get('ps')=" + c.Ctx.Params().Get("ps")
}

func (c *testControllerHandle) CustomWithParameter(param1 string) string {
	return param1
}

func (c *testControllerHandle) CustomWithParameters(param1, param2 string) string {
	// it returns empty string for requested path: /custom3/value1,
	// see BeforeActivation.
	return param1 + param2
}

type testSmallController struct{}

// test ctx + id in the same time.
func (c *testSmallController) GetHiParamEmptyInputWithCtxBy(ctx context.Context, id string) string {
	return "empty in but served with ctx.Params.Get('param2')= " + ctx.Params().Get("param2") + " == id == " + id
}

func TestControllerHandle(t *testing.T) {
	app := iris.New()
	m := New(app)
	m.Register(&TestServiceImpl{prefix: "service:"})
	m.Handle(new(testControllerHandle))
	m.Handle(new(testSmallController))

	e := httptest.New(t, app)

	// test the index, is not part of the current package's implementation but do it.
	e.GET("/").Expect().Status(httptest.StatusOK).Body().Equal("index")

	// the important things now.

	// this test ensures that the BeginRequest of the controller will be
	// called correctly and also the controller is binded to the first input argument
	// (which is the function's receiver, if any, in this case the *testController in go).
	expectedReqField := "this is a request field filled by this url param"
	e.GET("/histatic").WithQuery("reqfield", expectedReqField).Expect().Status(httptest.StatusOK).
		Body().Equal(expectedReqField)
	// this test makes sure that the binded values of the controller is handled correctly
	// and can be used in a user-defined, dynamic "mvc handler".
	e.GET("/hiservice").Expect().Status(httptest.StatusOK).
		Body().Equal("service: hi")
	e.GET("/hiservice/value").Expect().Status(httptest.StatusOK).
		Body().Equal("service: hi with param: value")
	// this worked with a temporary variadic on the resolvemethodfunc which is not
	// correct design, I should split the path and params with the rest of implementation
	// in order a simple template.Src can be given.
	e.GET("/hiparam/value").Expect().Status(httptest.StatusOK).
		Body().Equal("value")
	e.GET("/hiparamempyinput/value").Expect().Status(httptest.StatusOK).
		Body().Equal("empty in but served with ctx.Params.Get('ps')=value")
	e.GET("/custom/value1").Expect().Status(httptest.StatusOK).
		Body().Equal("value1")
	e.GET("/custom2/value2").Expect().Status(httptest.StatusOK).
		Body().Equal("value2")
	e.GET("/custom3/value1/value2").Expect().Status(httptest.StatusOK).
		Body().Equal("value1value2")
	e.GET("/custom3/value1").Expect().Status(httptest.StatusOK).
		Body().Equal("value1")

	e.GET("/hi/param/empty/input/with/ctx/value").Expect().Status(httptest.StatusOK).
		Body().Equal("empty in but served with ctx.Params.Get('param2')= value == id == value")
}

type testControllerHandleWithDynamicPathPrefix struct {
	Ctx iris.Context
}

func (c *testControllerHandleWithDynamicPathPrefix) GetBy(id string) string {
	params := c.Ctx.Params()
	return params.Get("model") + params.Get("action") + id
}

func TestControllerHandleWithDynamicPathPrefix(t *testing.T) {
	app := iris.New()
	New(app.Party("/api/data/{model:string}/{action:string}")).Handle(new(testControllerHandleWithDynamicPathPrefix))
	e := httptest.New(t, app)
	e.GET("/api/data/mymodel/myaction/myid").Expect().Status(httptest.StatusOK).
		Body().Equal("mymodelmyactionmyid")
}
