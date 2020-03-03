package redis

import ()

const (
	ResultUsed = 0
	ResultFree = 1
)

type Result struct {
	Data interface{}
	Cmd  *Command
	Err  error
}

func (r *Result) release() {

}
