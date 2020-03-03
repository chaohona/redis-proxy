package mvc

import (
	"reflect"

	"github.com/kataras/iris/v12/context"
	"github.com/kataras/iris/v12/macro"
)

func getPathParamsForInput(startParamIndex int, params []macro.TemplateParam, funcIn ...reflect.Type) (values []reflect.Value) {
	if len(funcIn) == 0 || len(params) == 0 {
		return
	}

	// consumedParams := make(map[int]bool, 0)
	// for _, in := range funcIn {
	// 	for j, p := range params {
	// 		if _, consumed := consumedParams[j]; consumed {
	// 			continue
	// 		}

	// 		// 	fmt.Printf("%s input arg type vs %s param type\n", in.Kind().String(), p.Type.Kind().String())
	// 		if m := macros.Lookup(p.Type); m != nil && m.GoType == in.Kind() {
	// 			consumedParams[j] = true
	// 			// fmt.Printf("param.go: bind path param func for paramName = '%s' and paramType = '%s'\n", paramName, paramType.String())
	// 			funcDep, ok := context.ParamResolverByKindAndIndex(m.GoType, p.Index)
	// 			//	funcDep, ok := context.ParamResolverByKindAndKey(in.Kind(), paramName)
	// 			if !ok {
	// 				// here we can add a logger about invalid parameter type although it should never happen here
	// 				// unless the end-developer modified the macro/macros with a special type but not the context/ParamResolvers.
	// 				continue
	// 			}
	// 			values = append(values, funcDep)
	// 		}
	// 	}
	// }

	consumed := make(map[int]struct{})
	for _, in := range funcIn {
		for j, param := range params {
			if _, ok := consumed[j]; ok {
				continue
			}

			funcDep, ok := context.ParamResolverByTypeAndIndex(in, startParamIndex+param.Index)
			if !ok {
				continue
			}

			values = append(values, funcDep)
			consumed[j] = struct{}{}
			break
		}
	}

	return
}
