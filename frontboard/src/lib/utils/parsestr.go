package utils

import (
	"reflect"
	"strconv"
	"strings"
)

type IntTuple2 struct {
	Item1 int
	Item2 int
}

type IntTuple3 struct {
	Item1 int
	Item2 int
	Item3 int
}

func StrRemoveEmptySlice(str, sep string) []string {
	var tmp []string = strings.Split(str, sep)
	return StrListRemoveEmpty(tmp)
}

func StrRemoveEmpty(str string) string {
	return strings.Replace(str, " ", "", -1)
}

func StrListRemoveEmpty(strs []string) (results []string) {
	var tmpStr string
	for _, str := range strs {
		tmpStr = StrRemoveEmpty(str)
		if tmpStr == "" {
			continue
		}
		results = append(results, tmpStr)
	}
	return
}

func StrToInt(str string) int {
	v, _ := strconv.ParseInt(str, 10, 64)
	return int(v)
}

func StrToInt32(str string) int32 {
	v, _ := strconv.ParseInt(str, 10, 64)
	return int32(v)
}

func StrToInt64(str string) int64 {
	v, _ := strconv.ParseInt(str, 10, 64)
	return v
}

func StrToUint32(str string) uint32 {
	v, _ := strconv.ParseUint(str, 10, 64)
	return uint32(v)
}

func StrToUint64(str string) uint64 {
	v, _ := strconv.ParseUint(str, 10, 64)
	return v
}

func StrToFloat64(str string) float64 {
	v, _ := strconv.ParseFloat(str, 10)
	return v
}

func InterfaceIsNil(in interface{}) bool {
	vi := reflect.ValueOf(in)
	if vi.Kind() == reflect.Ptr {
		return vi.IsNil()
	}
	return false
}
