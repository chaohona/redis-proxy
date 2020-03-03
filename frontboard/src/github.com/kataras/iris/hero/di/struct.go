package di

import (
	"fmt"
	"reflect"
	"sort"

	"github.com/kataras/iris/v12/context"
)

// Scope is the struct injector's struct value scope/permant state.
// See `Stateless` and `Singleton`.
type Scope uint8

const (
	// Stateless is the scope that the struct should be different on each binding,
	// think it like `Request Scoped`, per-request struct for mvc.
	Stateless Scope = iota
	// Singleton is the scope that the struct is the same
	// between calls, it has no dynamic dependencies or
	// any unexported fields that is not seted on creation,
	// so it doesn't need to be created on each call/request.
	Singleton
)

// read-only on runtime.
var scopeNames = map[Scope]string{
	Stateless: "Stateless",
	Singleton: "Singleton",
}

// Return "Stateless" for 0  or "Singleton" for 1.
func (scope Scope) String() string {
	name, ok := scopeNames[scope]
	if !ok {
		return "Unknown"
	}

	return name
}

type (
	targetStructField struct {
		Object     *BindObject
		FieldIndex []int
		// ValueIndex is used mostly for debugging, it's the order of the registered binded value targets to that field.
		ValueIndex int
	}

	// StructInjector keeps the data that are needed in order to do the binding injection
	// as fast as possible and with the best possible and safest way.
	StructInjector struct {
		initRef        reflect.Value
		initRefAsSlice []reflect.Value // useful when the struct is passed on a func as input args via reflection.
		elemType       reflect.Type
		//
		fields []*targetStructField
		// is true when contains bindable fields and it's a valid target struct,
		// it maybe 0 but struct may contain unexported fields or exported but no bindable (Stateless)
		// see `setState`.
		Has       bool
		CanInject bool // if any bindable fields when the state is NOT singleton.
		Scope     Scope

		FallbackBinder FallbackBinder
		ErrorHandler   ErrorHandler
	}
)

func (s *StructInjector) countBindType(typ BindType) (n int) {
	for _, f := range s.fields {
		if f.Object.BindType == typ {
			n++
		}
	}
	return
}

// Sorter is the type for sort customization of a struct's fields
// and its available bindable values.
//
// Sorting applies only when a field can accept more than one registered value.
type Sorter func(t1 reflect.Type, t2 reflect.Type) bool

// SortByNumMethods is a builtin sorter to sort fields and values
// based on their type and its number of methods, highest number of methods goes first.
//
// It is the default sorter on package-level struct injector function `Struct`.
var SortByNumMethods Sorter = func(t1 reflect.Type, t2 reflect.Type) bool {
	if t1.Kind() != t2.Kind() {
		return true
	}

	if k := t1.Kind(); k == reflect.Interface || k == reflect.Struct {
		return t1.NumMethod() > t2.NumMethod()
	} else if k != reflect.Struct {
		return false // non-structs goes last.
	}

	return true
}

// MakeStructInjector returns a new struct injector, which will be the object
// that the caller should use to bind exported fields or
// embedded unexported fields that contain exported fields
// of the "v" struct value or pointer.
//
// The hijack and the goodFunc are optional, the "values" is the dependencies collection.
func MakeStructInjector(v reflect.Value, sorter Sorter, values ...reflect.Value) *StructInjector {
	s := &StructInjector{
		initRef:        v,
		initRefAsSlice: []reflect.Value{v},
		elemType:       IndirectType(v.Type()),
		FallbackBinder: DefaultFallbackBinder,
		ErrorHandler:   DefaultErrorHandler,
	}

	// Optionally check and keep good values only here,
	// but not required because they are already checked by users of this function.
	//
	// for i, v := range values {
	// 	if !goodVal(v) || IsZero(v) {
	// 		if last := len(values) - 1; last > i {
	// 			values = append(values[:i], values[i+1:]...)
	// 		} else {
	// 			values = values[0:last]
	// 		}
	// 	}
	// }

	visited := make(map[int]struct{}) // add a visited to not add twice a single value (09-Jul-2019).
	fields := lookupFields(s.elemType, true, nil)

	// for idx, val := range values {
	// 	  fmt.Printf("[%d] value type [%s] value name [%s]\n", idx, val.Type().String(), val.String())
	// }

	if len(fields) > 1 && sorter != nil {
		sort.Slice(fields, func(i, j int) bool {
			return sorter(fields[i].Type, fields[j].Type)
		})
	}

	for _, f := range fields {
		// fmt.Printf("[%d] field type [%s] value name [%s]\n", idx, f.Type.String(), f.Name)
		if b, ok := tryBindContext(f.Type); ok {
			s.fields = append(s.fields, &targetStructField{
				FieldIndex: f.Index,
				Object:     b,
			})
			continue
		}

		var possibleValues []*targetStructField

		for idx, val := range values {
			if _, alreadySet := visited[idx]; alreadySet {
				continue
			}

			// the binded values to the struct's fields.
			b, err := MakeBindObject(val, nil)
			if err != nil {
				panic(err)
				// return s // if error stop here.
			}

			if b.IsAssignable(f.Type) {
				possibleValues = append(possibleValues, &targetStructField{
					ValueIndex: idx,
					FieldIndex: f.Index,
					Object:     &b,
				})
			}
		}

		if l := len(possibleValues); l == 0 {
			continue
		} else if l > 1 && sorter != nil {
			sort.Slice(possibleValues, func(i, j int) bool {
				// if first.Object.BindType != second.Object.BindType {
				// 	return true
				// }

				// if first.Object.BindType != Static { // dynamic goes last.
				// 	return false
				// }
				return sorter(possibleValues[i].Object.Type, possibleValues[j].Object.Type)
			})
		}

		tf := possibleValues[0]
		visited[tf.ValueIndex] = struct{}{}
		// fmt.Printf("bind the object to the field: %s at index: %#v and type: %s\n", f.Name, f.Index, f.Type.String())
		s.fields = append(s.fields, tf)
	}

	s.Has = len(s.fields) > 0
	// set the overall state of this injector.
	s.fillStruct()
	s.setState()

	return s
}

// set the state, once.
// Here the "initRef" have already the static bindings and the manually-filled fields.
func (s *StructInjector) setState() {
	// note for zero length of struct's fields:
	// if struct doesn't contain any field
	// so both of the below variables will be 0,
	// so it's a singleton.
	// At the other hand the `s.HasFields` maybe false
	// but the struct may contain UNEXPORTED fields or non-bindable fields (request-scoped on both cases)
	// so a new controller/struct at the caller side should be initialized on each request,
	// we should not depend on the `HasFields` for singleton or no, this is the reason I
	// added the `.State` now.

	staticBindingsFieldsLength := s.countBindType(Static)
	allStructFieldsLength := NumFields(s.elemType, false)
	// check if unexported(and exported) fields are set-ed manually or via binding (at this time we have all fields set-ed inside the "initRef")
	// i.e &Controller{unexportedField: "my value"}
	// or dependencies values = "my value" and Controller struct {Field string}
	// if so then set the temp staticBindingsFieldsLength to that number, so for example:
	// if static binding length is 0
	// but an unexported field is set-ed then act that as singleton.

	if allStructFieldsLength > staticBindingsFieldsLength {
		structFieldsUnexportedNonZero := LookupNonZeroFieldsValues(s.initRef, false)
		staticBindingsFieldsLength = len(structFieldsUnexportedNonZero)
	}

	// println("allStructFieldsLength: ", allStructFieldsLength)
	// println("staticBindingsFieldsLength: ", staticBindingsFieldsLength)

	// if the number of static values binded is equal to the
	// total struct's fields(including unexported fields this time) then set as singleton.
	if staticBindingsFieldsLength == allStructFieldsLength {
		s.Scope = Singleton
		// the default is `Stateless`, which means that a new instance should be created
		//  on each inject action by the caller.
		return
	}

	s.CanInject = s.Scope == Stateless && s.Has
}

// fill the static bindings values once.
func (s *StructInjector) fillStruct() {
	if !s.Has {
		return
	}
	// if field is Static then set it to the value that passed by the caller,
	// so will have the static bindings already and we can just use that value instead
	// of creating new instance.
	destElem := IndirectValue(s.initRef)
	for _, f := range s.fields {
		// if field is Static then set it to the value that passed by the caller,
		// so will have the static bindings already and we can just use that value instead
		// of creating new instance.
		if f.Object.BindType == Static {
			destElem.FieldByIndex(f.FieldIndex).Set(f.Object.Value)
		}
	}
}

// String returns a debug trace message.
func (s *StructInjector) String() (trace string) {
	for i, f := range s.fields {
		elemField := s.elemType.FieldByIndex(f.FieldIndex)

		format := "\t[%d] %s binding: %#+v for field '%s %s'"
		if len(s.fields) > i+1 {
			format += "\n"
		}

		if !f.Object.Value.IsValid() {
			continue // probably a Context.
		}

		valuePresent := f.Object.Value.Interface()

		if f.Object.BindType == Dynamic {
			valuePresent = f.Object.Type.String()
		}

		trace += fmt.Sprintf(format, i+1, bindTypeString(f.Object.BindType), valuePresent, elemField.Name, elemField.Type.String())
	}

	return
}

// Inject accepts a destination struct and any optional context value(s),
// hero and mvc takes only one context value and this is the `context.Context`.
// It applies the bindings to the "dest" struct. It calls the InjectElem.
func (s *StructInjector) Inject(ctx context.Context, dest interface{}) {
	if dest == nil {
		return
	}

	v := IndirectValue(ValueOf(dest))
	s.InjectElem(ctx, v)
}

// InjectElem same as `Inject` but accepts a reflect.Value and bind the necessary fields directly.
func (s *StructInjector) InjectElem(ctx context.Context, destElem reflect.Value) {
	for _, f := range s.fields {
		f.Object.Assign(ctx, func(v reflect.Value) {
			ff := destElem.FieldByIndex(f.FieldIndex)
			if !v.Type().AssignableTo(ff.Type()) {
				return
			}

			destElem.FieldByIndex(f.FieldIndex).Set(v)
		})
	}
}

// Acquire returns a new value of the struct or
// the same struct that is used for resolving the dependencies.
// If the scope is marked as singleton then it returns the first instance,
// otherwise it creates new and returns it.
//
// See `Singleton` and `Stateless` for more.
func (s *StructInjector) Acquire() reflect.Value {
	if s.Scope == Singleton {
		return s.initRef
	}
	return reflect.New(s.elemType)
}

// AcquireSlice same as `Acquire` but it returns a slice of
// values structs, this can be used when a struct is passed as an input parameter
// on a function, again if singleton then it returns a pre-created slice which contains
// the first struct value given by the struct injector's user.
func (s *StructInjector) AcquireSlice() []reflect.Value {
	if s.Scope == Singleton {
		return s.initRefAsSlice
	}
	return []reflect.Value{reflect.New(s.elemType)}
}
