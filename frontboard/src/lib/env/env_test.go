package env

import (
	"testing"
)

func TestEnv(t *testing.T) {
	Load("./test.json")
	t.Log(configData)
}
