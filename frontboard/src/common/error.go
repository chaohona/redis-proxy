package common

const (
	ERR_OK                = 0
	ERR_DB                = 1 // db错误
	ERR_REDIS_POOLS_EMPTY = 2 // Redis地址池子用完了
	ERR_URL_PARSE         = 3 // 参数解析错误
	ERR_INVALID_REDIS     = 4 // 不存在的Redis
)

type ErrInfo struct {
	Ret     int
	RetInfo string
}

var errMap map[int]string
var errInfoList = []ErrInfo{
	{ERR_OK, "okay"},
	{ERR_DB, "db error"},
	{ERR_REDIS_POOLS_EMPTY, "can not create more redis"},
	{ERR_URL_PARSE, "parse quest failed"},
	{ERR_INVALID_REDIS, "redis not exists"},
}

func init() {
	errMap = make(map[int]string)
	for _, info := range errInfoList {
		errMap[info.Ret] = info.RetInfo
	}
}

func ErrDesc(err int) string {
	return errMap[err]
}

func GetErrInfo(ret int) ErrInfo {
	return ErrInfo{
		Ret:     ret,
		RetInfo: errMap[ret],
	}
}
