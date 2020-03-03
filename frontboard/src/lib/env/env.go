package env

import (
	"encoding/json"
	"io/ioutil"
	"lib/glog"
	"strings"
)

var configData map[string]map[string]string

func Load(path string) bool {
	file, err := ioutil.ReadFile(path)
	if err != nil {
		glog.Error("[配置] 读取失败 ", path, ",", err)
		return false
	}
	orgStrs := strings.Split(string(file), "\n")
	var dstStr string
	_ = dstStr
	for _, str := range orgStrs {
		tmpList := strings.Split(str, "//")
		if len(tmpList) > 0 {
			dstStr += tmpList[0]
		}
	}
	//fmt.Println("orgstr:", orgStrs)
	err = json.Unmarshal([]byte(dstStr), &configData)
	if err != nil {
		glog.Error("[配置] 解析失败 ", path, ",", err)
		return false
	}
	return true
}

func Get(table, key string) string {
	t, ok := configData[table]
	if !ok {
		return ""
	}
	val, ok := t[key]
	if !ok {
		return ""
	}
	return val
}
