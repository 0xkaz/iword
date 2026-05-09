package iword_test

// Integration tests for the iwordserver Client.
//
// Requires iwordserver running with the test dictionary:
//
//	bin/iwordctl load /tmp/dict.txt
//	bin/iwordserver -u /tmp/iword_test_client.sock -p 0 &
//	go test ./bindings/go/ -run TestClient
//
// In CI, the server is started by the Go binding job before running tests.
// Tests are skipped automatically if the socket is not available.

import (
	"fmt"
	"os"
	"strings"
	"testing"

	iword "github.com/0xkaz/iword/bindings/go"
)

const testSockEnv = "IWORD_SERVER_SOCK"
const testSockDefault = "/tmp/iword_test_client.sock"

func newTestClient(t *testing.T) *iword.Client {
	t.Helper()
	sock := os.Getenv(testSockEnv)
	if sock == "" {
		sock = testSockDefault
	}
	c, err := iword.NewClient("unix", sock)
	if err != nil {
		t.Skipf("iwordserver not available at %s: %v", sock, err)
	}
	t.Cleanup(func() { c.Close() })
	return c
}

// ---- Ping ---------------------------------------------------------------

func TestClientPing(t *testing.T) {
	c := newTestClient(t)
	if err := c.Ping(); err != nil {
		t.Errorf("Ping() error: %v", err)
	}
}

// ---- Seek ---------------------------------------------------------------

func TestClientSeekFound(t *testing.T) {
	c := newTestClient(t)
	key, err := c.Seek("free")
	if err != nil {
		t.Fatalf("Seek error: %v", err)
	}
	if key != iword.KeySpam {
		t.Errorf("Seek(\"free\") = %d, want %d (KeySpam)", key, iword.KeySpam)
	}
}

func TestClientSeekAdult(t *testing.T) {
	c := newTestClient(t)
	key, err := c.Seek("adult_word")
	if err != nil {
		t.Fatalf("Seek error: %v", err)
	}
	if key != iword.KeyAdult {
		t.Errorf("Seek(\"adult_word\") = %d, want %d (KeyAdult)", key, iword.KeyAdult)
	}
}

func TestClientSeekNotFound(t *testing.T) {
	c := newTestClient(t)
	key, err := c.Seek("notaword_xyz_abc")
	if err != nil {
		t.Fatalf("Seek error: %v", err)
	}
	if key != -1 {
		t.Errorf("Seek(\"notaword_xyz_abc\") = %d, want -1", key)
	}
}

func TestClientSeekEmpty(t *testing.T) {
	c := newTestClient(t)
	key, err := c.Seek("")
	if err != nil {
		t.Fatalf("Seek(\"\") error: %v", err)
	}
	if key != -1 {
		t.Errorf("Seek(\"\") = %d, want -1", key)
	}
}

// ---- Map ----------------------------------------------------------------

func TestClientMapReturnsMatches(t *testing.T) {
	c := newTestClient(t)
	matches, err := c.Map("get free prize now", iword.ModeHTML|iword.ModeForbid)
	if err != nil {
		t.Fatalf("Map error: %v", err)
	}
	if len(matches) == 0 {
		t.Fatal("Map returned no matches for spam text")
	}
	for _, m := range matches {
		if m.Position < 0 {
			t.Errorf("match has negative Position: %+v", m)
		}
		if m.Length <= 0 {
			t.Errorf("match has non-positive Length: %+v", m)
		}
	}
}

func TestClientMapEmpty(t *testing.T) {
	c := newTestClient(t)
	matches, err := c.Map("", iword.ModeHTML|iword.ModeForbid)
	if err != nil {
		t.Fatalf("Map(\"\") error: %v", err)
	}
	if len(matches) != 0 {
		t.Errorf("Map(\"\") returned %d matches, want 0", len(matches))
	}
}

func TestClientMapPositionAndLength(t *testing.T) {
	c := newTestClient(t)
	text := "hello free world"
	matches, err := c.Map(text, iword.ModeHTML|iword.ModeForbid)
	if err != nil {
		t.Fatalf("Map error: %v", err)
	}
	for _, m := range matches {
		if m.Position+m.Length > len(text) {
			t.Errorf("match overflows text: pos=%d len=%d textlen=%d", m.Position, m.Length, len(text))
		}
	}
}

// ---- Mask ---------------------------------------------------------------

func TestClientMask(t *testing.T) {
	c := newTestClient(t)
	mask, err := c.Mask()
	if err != nil {
		t.Fatalf("Mask error: %v", err)
	}
	if mask == 0 {
		t.Error("Mask() returned 0; dictionary may not be loaded")
	}
	if mask&(1<<iword.KeySpam) == 0 {
		t.Errorf("Mask() missing KeySpam bit: mask=0x%x", mask)
	}
}

// ---- FilterText ---------------------------------------------------------

func TestClientFilterText(t *testing.T) {
	c := newTestClient(t)
	input := "This is free spam"
	output, err := c.FilterText(input, iword.ModeHTML|iword.ModeForbid)
	if err != nil {
		t.Fatalf("FilterText error: %v", err)
	}
	if output == input {
		t.Error("FilterText did not modify spam text")
	}
	if !strings.Contains(output, "*") {
		t.Errorf("FilterText output contains no '*': %q", output)
	}
	if len(output) != len(input) {
		t.Errorf("FilterText changed byte length: %d → %d", len(input), len(output))
	}
}

func TestClientFilterTextClean(t *testing.T) {
	c := newTestClient(t)
	input := "hello world"
	output, err := c.FilterText(input, iword.ModeHTML|iword.ModeForbid)
	if err != nil {
		t.Fatalf("FilterText error: %v", err)
	}
	if output != input {
		t.Errorf("FilterText modified clean text: got %q, want %q", output, input)
	}
}

// ---- Concurrency --------------------------------------------------------

func TestClientConcurrent(t *testing.T) {
	// Client.mu serializes calls; safe to call from multiple goroutines
	// on the same Client instance.
	c := newTestClient(t)
	const goroutines = 20
	errs := make(chan error, goroutines)
	done := make(chan struct{})

	for i := 0; i < goroutines; i++ {
		go func() {
			defer func() { done <- struct{}{} }()
			key, err := c.Seek("free")
			if err != nil {
				errs <- err
				return
			}
			if key != iword.KeySpam {
				errs <- fmt.Errorf("Seek returned %d, want %d", key, iword.KeySpam)
			}
		}()
	}
	for i := 0; i < goroutines; i++ {
		<-done
	}
	close(errs)
	for err := range errs {
		t.Error(err)
	}
}
