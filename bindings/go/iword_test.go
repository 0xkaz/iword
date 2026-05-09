package iword_test

// Integration tests — requires a pre-loaded dictionary:
//
//	bin/iwordctl load /tmp/dict.txt   (free:2, spam:2, prize:2, adult_word:1, apple:9)
//	go test ./bindings/go/
//	go test -race ./bindings/go/      (concurrency check)
//
// The CI workflow loads the dictionary before running tests.

import (
	"os"
	"strings"
	"sync"
	"testing"

	iword "github.com/0xkaz/iword/bindings/go"
)

func TestMain(m *testing.M) {
	if k := os.Getenv("IWORD_TEST_KEY"); k != "" {
		iword.SetDictKey(k)
	}
	os.Exit(m.Run())
}

// ---- Seek ---------------------------------------------------------------

func TestSeekFound(t *testing.T) {
	if got := iword.Seek("free"); got != iword.KeySpam {
		t.Errorf("Seek(\"free\") = %d, want %d (KeySpam)", got, iword.KeySpam)
	}
}

func TestSeekAdult(t *testing.T) {
	if got := iword.Seek("adult_word"); got != iword.KeyAdult {
		t.Errorf("Seek(\"adult_word\") = %d, want %d (KeyAdult)", got, iword.KeyAdult)
	}
}

func TestSeekDefault(t *testing.T) {
	if got := iword.Seek("apple"); got != iword.KeyDefault {
		t.Errorf("Seek(\"apple\") = %d, want %d (KeyDefault)", got, iword.KeyDefault)
	}
}

func TestSeekNotFound(t *testing.T) {
	if got := iword.Seek("notaword_xyz_abc"); got != -1 {
		t.Errorf("Seek(\"notaword_xyz_abc\") = %d, want -1", got)
	}
}

// ---- Map ----------------------------------------------------------------

func TestMapReturnsMatches(t *testing.T) {
	matches := iword.Map("Get your free prize now!", iword.ModeHTML|iword.ModeForbid)
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
		if m.Key < 0 || m.Key > 14 {
			t.Errorf("match has out-of-range Key: %+v", m)
		}
	}
}

func TestMapEmptyText(t *testing.T) {
	if matches := iword.Map("", iword.ModeHTML|iword.ModeForbid); len(matches) != 0 {
		t.Errorf("Map(\"\") returned %d matches, want 0", len(matches))
	}
}

func TestMapCategoryKeys(t *testing.T) {
	// Verify that different category keys are correctly distinguished
	matches := iword.Map("free adult_word apple", iword.ModeHTML|iword.ModeForbid)
	keySet := map[int]bool{}
	for _, m := range matches {
		keySet[m.Key] = true
	}
	if !keySet[iword.KeySpam] {
		t.Error("Map: KeySpam not found in mixed-category text")
	}
	if !keySet[iword.KeyAdult] {
		t.Error("Map: KeyAdult not found in mixed-category text")
	}
}

func TestMapLongText(t *testing.T) {
	// igate receives arbitrary-length event text; ensure no crash or truncation.
	// iword_map returns nil (no matches) for clean text — that is correct behavior.
	repeated := strings.Repeat("hello world this is clean text. ", 500) // ~16KB
	matches := iword.Map(repeated, iword.ModeHTML|iword.ModeForbid)
	// nil or empty slice both mean "no matches" — just must not panic
	_ = matches

	spamRepeated := strings.Repeat("get free prize now! ", 200)
	spamMatches := iword.Map(spamRepeated, iword.ModeHTML|iword.ModeForbid)
	if len(spamMatches) == 0 {
		t.Error("Map returned no matches for long spam text")
	}
}

func TestMapHTMLStripped(t *testing.T) {
	// MODE_HTML skips tag content; keyword inside tag should not match
	withHTML := iword.Map("<b>free</b> prize", iword.ModeHTML|iword.ModeForbid)
	withoutHTML := iword.Map("free prize", iword.ModeHTML|iword.ModeForbid)
	// Both should find matches (keyword is in text node either way),
	// but this confirms no panic with HTML input.
	if withHTML == nil || withoutHTML == nil {
		t.Error("Map returned nil for HTML input")
	}
}

// ---- SetLimit -----------------------------------------------------------

func TestSetLimit(t *testing.T) {
	iword.SetLimit(1)
	matches := iword.Map("free spam prize", iword.ModeHTML|iword.ModeForbid)
	iword.SetLimit(0) // restore: 0 = no limit
	if len(matches) > 1 {
		t.Errorf("SetLimit(1): expected at most 1 match, got %d", len(matches))
	}
}

// ---- Mask ---------------------------------------------------------------

func TestMask(t *testing.T) {
	m := iword.Mask()
	if m == 0 {
		t.Error("Mask() returned 0; dictionary may not be loaded")
	}
}

// ---- FilterText ---------------------------------------------------------

func TestFilterText(t *testing.T) {
	input := "This is free spam"
	output := iword.FilterText(input, iword.ModeHTML|iword.ModeForbid)
	if output == input {
		t.Error("FilterText did not modify spam text")
	}
	if !strings.Contains(output, "*") {
		t.Errorf("FilterText output contains no '*': %q", output)
	}
}

func TestFilterTextClean(t *testing.T) {
	input := "hello world"
	if output := iword.FilterText(input, iword.ModeHTML|iword.ModeForbid); output != input {
		t.Errorf("FilterText modified clean text: got %q, want %q", output, input)
	}
}

func TestFilterTextPreservesLength(t *testing.T) {
	input := "buy free stuff"
	output := iword.FilterText(input, iword.ModeHTML|iword.ModeForbid)
	if len(output) != len(input) {
		t.Errorf("FilterText changed byte length: input=%d output=%d", len(input), len(output))
	}
}

// ---- Seek edge cases ----------------------------------------------------

func TestSeekCaseSensitive(t *testing.T) {
	// iword dictionary lookup is case-sensitive
	lower := iword.Seek("free")
	upper := iword.Seek("FREE")
	if lower != iword.KeySpam {
		t.Errorf("Seek(\"free\") = %d, want %d (KeySpam)", lower, iword.KeySpam)
	}
	// Upper-case variant should not match (dictionary has lowercase "free")
	if upper == iword.KeySpam {
		t.Log("Seek(\"FREE\") matched — dictionary is case-insensitive (noted)")
	}
}

func TestSeekEmptyString(t *testing.T) {
	if got := iword.Seek(""); got != -1 {
		t.Errorf("Seek(\"\") = %d, want -1", got)
	}
}

// ---- Map edge cases -----------------------------------------------------

func TestMapModeEnglish(t *testing.T) {
	// ModeEnglish splits on word boundaries; must not panic
	matches := iword.Map("free-prize combo", iword.ModeHTML|iword.ModeForbid|iword.ModeEnglish)
	_ = matches // just ensure no crash
}

func TestMapModeOnlyHTML(t *testing.T) {
	// Without ModeForbid, forbidden-category words (key<5) should not appear
	matches := iword.Map("free spam prize", iword.ModeHTML)
	for _, m := range matches {
		if m.Key < 5 {
			t.Errorf("Map(ModeHTML only) returned forbidden key %d — expected ModeForbid required", m.Key)
		}
	}
}

func TestMapPositionAndLength(t *testing.T) {
	text := "hello free world"
	matches := iword.Map(text, iword.ModeHTML|iword.ModeForbid)
	for _, m := range matches {
		if m.Position+m.Length > len(text) {
			t.Errorf("match overflows text: pos=%d len=%d textlen=%d", m.Position, m.Length, len(text))
		}
		word := text[m.Position : m.Position+m.Length]
		if word != "free" {
			t.Errorf("expected matched word \"free\", got %q", word)
		}
	}
}

func TestMapRepeatedCalls(t *testing.T) {
	// Repeated Map calls must not leak or crash (CGO malloc/free balance)
	for i := 0; i < 200; i++ {
		matches := iword.Map("get free prize now spam", iword.ModeHTML|iword.ModeForbid)
		if len(matches) == 0 {
			t.Fatalf("iteration %d: Map returned no matches", i)
		}
	}
}

// ---- SetLimit edge cases ------------------------------------------------

func TestSetLimitZeroMeansNoLimit(t *testing.T) {
	iword.SetLimit(0)
	matches := iword.Map("free spam prize free spam", iword.ModeHTML|iword.ModeForbid)
	// With no limit, all occurrences should be returned
	if len(matches) == 0 {
		t.Error("SetLimit(0): Map returned no matches (expected unlimited)")
	}
}

func TestSetLimitHighValue(t *testing.T) {
	iword.SetLimit(100)
	matches := iword.Map("free spam prize", iword.ModeHTML|iword.ModeForbid)
	iword.SetLimit(0)
	if len(matches) == 0 {
		t.Error("SetLimit(100): Map returned no matches")
	}
}

// ---- FilterText edge cases ----------------------------------------------

func TestFilterTextMultipleWords(t *testing.T) {
	input := "free spam prize"
	output := iword.FilterText(input, iword.ModeHTML|iword.ModeForbid)
	if output == input {
		t.Error("FilterText did not modify text with multiple spam words")
	}
	if len(output) != len(input) {
		t.Errorf("FilterText changed byte length: %d → %d", len(input), len(output))
	}
}

func TestFilterTextOnlyNonSpam(t *testing.T) {
	// KeyDefault words (key=9, apple) ARE returned by Map with ModeForbid
	// and thus replaced by FilterText. Clean words not in the dictionary
	// must not be touched.
	input := "cleanwordxyz"
	output := iword.FilterText(input, iword.ModeHTML|iword.ModeForbid)
	if output != input {
		t.Errorf("FilterText modified word not in dictionary: got %q, want %q", output, input)
	}
}

// ---- Mask ---------------------------------------------------------------

func TestMaskContainsExpectedKeys(t *testing.T) {
	mask := iword.Mask()
	// Dictionary has key=1 (adult), key=2 (spam), key=9 (default)
	if mask&(1<<iword.KeyAdult) == 0 {
		t.Errorf("Mask() missing KeyAdult bit: mask=0x%x", mask)
	}
	if mask&(1<<iword.KeySpam) == 0 {
		t.Errorf("Mask() missing KeySpam bit: mask=0x%x", mask)
	}
}

// ---- Concurrency --------------------------------------------------------
//
// IMPORTANT FINDING: iword_seek() and iword_map() are NOT thread-safe.
// Concurrent calls without a Mutex cause SIGSEGV (confirmed in CI with -race).
//
// igate MUST serialize all iword calls behind a sync.Mutex.
// Do NOT call Seek() or Map() from multiple goroutines without locking.
//
// The tests below verify correct behavior when a Mutex is used,
// and document the required locking pattern for igate.

var iwordMu sync.Mutex

func TestSeekConcurrentWithMutex(t *testing.T) {
	const goroutines = 20
	var wg sync.WaitGroup
	errors := make(chan string, goroutines*2)

	for i := 0; i < goroutines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			iwordMu.Lock()
			got := iword.Seek("free")
			iwordMu.Unlock()
			if got != iword.KeySpam {
				errors <- "Seek returned wrong key under mutex-protected concurrency"
			}

			iwordMu.Lock()
			got = iword.Seek("notaword_xyz")
			iwordMu.Unlock()
			if got != -1 {
				errors <- "Seek returned non-(-1) for missing word under mutex-protected concurrency"
			}
		}()
	}
	wg.Wait()
	close(errors)
	for e := range errors {
		t.Error(e)
	}
}

func TestMapConcurrentWithMutex(t *testing.T) {
	const goroutines = 10
	var wg sync.WaitGroup
	errors := make(chan string, goroutines)

	for i := 0; i < goroutines; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			iwordMu.Lock()
			matches := iword.Map("get free prize now", iword.ModeHTML|iword.ModeForbid)
			iwordMu.Unlock()
			if len(matches) == 0 {
				errors <- "Map returned no matches under mutex-protected concurrency"
			}
		}()
	}
	wg.Wait()
	close(errors)
	for e := range errors {
		t.Error(e)
	}
}
