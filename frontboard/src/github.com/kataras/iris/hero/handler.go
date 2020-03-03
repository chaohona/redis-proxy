package hero

import (
	"fmt"
	"reflect"
	"runtime"

	"github.com/kataras/iris/v12/context"
	"github.com/kataras/iris/v12/hero/di"

	"github.com/kataras/golog"
)

// var genericFuncTyp = reflect.TypeOf(func(context.Context) reflect.Value { return reflect.Value{} })

// // IsGenericFunc reports whether the "inTyp" is a type of func(Context) interface{}.
// func IsGenericFunc(inTyp reflect.Type) bool {
// 	return inTyp == genericFuncTyp
// }

// checks if "handler" is context.Handler: func(context.Context).
func isContextHandler(handler interface{}) (context.Handler, bool) {
	h, ok := handler.(context.Handler)
	return h, ok
}

func validateHandler(handler interface{}) error {
	if typ := reflect.TypeOf(handler); !di.IsFunc(typ) {
		return fmt.Errorf("handler expected to be a kind of func but got typeof(%s)", typ.String())
	}
	return nil
}

// makeHandler accepts a "handler" function which can accept any input arguments that match
// with the "values" types and any output result, that matches the hero types, like string, int (string,int),
// custom structs, Result(View | Response) and anything that you can imagine,
// and returns a low-level `context/iris.Handler` which can be used anywhere in the Iris Application,
// as middleware or as simple route handler or party handler or subdomain handler-router.
func makeHandler(handler interface{}, errorHandler di.ErrorHandler, values ...reflect.Value) (context.Handler, error) {
	if err := validateHandler(handler); err != nil {
		return nil, err
	}

	if h, is := isContextHandler(handler); is {
		golog.Warnf("the standard API to register a context handler could be used instead")
		return h, nil
	}

	fn := reflect.ValueOf(handler)
	n := fn.Type().NumIn()

	if n == 0 {
		h := func(ctx context.Context) {
			DispatchFuncResult(ctx, nil, fn.Call(di.EmptyIn))
		}

		return h, nil
	}

	funcInjector := di.Func(fn, values...)
	funcInjector.ErrorHandler = errorHandler

	valid := funcInjector.Length == n

	if !valid {
		// is invalid when input len and values are not match
		// or their types are not match, we will take look at the
		// second statement, here we will re-try it
		// using binders for path parameters: string, int, int64, uint8, uint64, bool and so on.
		// We don't have access to the path, so neither to the macros here,
		// but in mvc. So we have to do it here.
		valid = funcInjector.Retry(new(params).resolve)
		if !valid {
			pc := fn.Pointer()
			fpc := runtime.FuncForPC(pc)
			callerFileName, callerLineNumber := fpc.FileLine(pc)
			callerName := fpc.Name()

			err := fmt.Errorf("input arguments length(%d) and valid binders length(%d) are not equal for typeof '%s' which is defined at %s:%d by %s",
				n, funcInjector.Length, fn.Type().String(), callerFileName, callerLineNumber, callerName)
			return nil, err
		}
	}

	h := func(ctx context.Context) {
		// in := make([]reflect.Value, n, n)
		// funcInjector.Inject(&in, reflect.ValueOf(ctx))
		// DispatchFuncResult(ctx, fn.Call(in))
		DispatchFuncResult(ctx, nil, funcInjector.Call(ctx))
	}

	return h, nil
}
