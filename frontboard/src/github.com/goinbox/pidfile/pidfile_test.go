package pidfile

import (
	"os"
	"os/exec"
	"os/signal"
	"syscall"
	"testing"
)

func TestReadWritePidFile(t *testing.T) {
	pid := NewPid(os.Getpid())
	file := NewFile("./pidfiletest.pid")

	err := WritePidToFile(file, pid)
	if err != nil {
		t.Fatalf("write pid %s to file %s failed, error: %s", pid.Id, file.Path, err.Error())
	}

	fpid, err := ReadPidFromFile(file)
	if err != nil {
		t.Fatalf("read pid from file %s failed, error: %s", file.Path, err.Error())
	}

	if fpid.Id != pid.Id {
		t.Fatalf("read pid error, expect %s got %s", pid.Id, fpid.Id)
	}

	if !fpid.ProcessExist() {
		t.Fatalf("process with pid %s not exist", fpid.Id)
	}

	if err = file.Remove(); err != nil {
		t.Fatalf("remove test file %s failed", file.Path)
	}
}

func TestCreateClearPidFile(t *testing.T) {
	pidfile := createPidFile(t)
	clearPidFile(pidfile, t)
}

func TestCrossCreateOrClearPidFile(t *testing.T) {
	pidfile := createPidFile(t)
	sc := make(chan os.Signal)

	isChildProcess := (os.Getenv("CHILD_PROCESS") == "1")
	if !isChildProcess {
		go func() {
			cmd := exec.Command(os.Args[0], "-test.run=TestCrossCreateOrClearPidFile")
			cmd.Env = append(os.Environ(), "CHILD_PROCESS=1")

			err := cmd.Run()
			if err != nil {
				t.Fatalf("run child process failed, error: %s, isChildProcess: %v", err.Error(), isChildProcess)
			}
		}()

		// wait signal from child process
		waitSignal(sc)

		// read child process pid from tmp file
		tmpPid, err := ReadPidFromFile(NewFile("./pidfiletest.pid.tmp"))
		if err != nil || !tmpPid.ProcessExist() {
			t.Fatalf("read pid of child process failed, error: %s, isChildProcess: %v", err.Error(), isChildProcess)
		}

		clearPidFile(pidfile, t)

		// send signal to child process
		sendSignal(tmpPid.Id, isChildProcess, t)
	} else {
		// send signal to parent process
		sendSignal(os.Getppid(), isChildProcess, t)

		// wait signal from parent process
		waitSignal(sc)
		clearPidFile(pidfile, t)
	}
}

func createPidFile(t *testing.T) *PidFile {
	path := "./pidfiletest.pid"
	pidfile, err := CreatePidFile(path)
	if err != nil {
		t.Fatalf("create pid file %s failed, error: %s", path, err.Error())
	}
	return pidfile
}

func clearPidFile(pidfile *PidFile, t *testing.T) {
	err := ClearPidFile(pidfile)
	if err != nil {
		t.Fatalf("clear pid file %s failed, error: %s", pidfile.Path, err.Error())
	}
}

func waitSignal(sc chan os.Signal) {
	signal.Notify(sc, syscall.SIGUSR1)
	<-sc
}

func sendSignal(pid int, isChildProcess bool, t *testing.T) {
	process, err := os.FindProcess(pid)
	if err != nil {
		t.Fatalf("find process by pid %s failed, error: %s, isChildProcess: %v", pid, err.Error(), isChildProcess)
	}

	err = process.Signal(syscall.SIGUSR1)
	if err != nil {
		t.Fatalf("send signal to process with pid %s failed, error: %s, isChildProcess: %v", pid, err.Error(), isChildProcess)
	}
}
