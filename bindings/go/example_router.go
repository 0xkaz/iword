//go:build ignore

// Example: Agent router using iWord for zero-latency keyword-based routing.
// Run: go run example_router.go (after loading dictionary with iwordctl)
package main

import (
	"encoding/json"
	"fmt"
	"net/http"

	iword "github.com/0xkaz/iword/bindings/go"
)

type SeekResponse struct {
	Word  string `json:"word"`
	Key   int    `json:"key"`
	Found bool   `json:"found"`
}

type MapResponse struct {
	Matches []iword.Match `json:"matches"`
	Count   int           `json:"count"`
}

func seekHandler(w http.ResponseWriter, r *http.Request) {
	word := r.URL.Query().Get("word")
	if word == "" {
		http.Error(w, "missing ?word=", http.StatusBadRequest)
		return
	}
	key := iword.Seek(word)
	resp := SeekResponse{
		Word:  word,
		Key:   key,
		Found: key >= 0,
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func mapHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "POST required", http.StatusMethodNotAllowed)
		return
	}
	var body struct {
		Text string `json:"text"`
		Mode int    `json:"mode"`
	}
	if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}
	if body.Mode == 0 {
		body.Mode = iword.ModeHTML | iword.ModeForbid
	}
	matches := iword.Map(body.Text, body.Mode)
	resp := MapResponse{Matches: matches, Count: len(matches)}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func main() {
	fmt.Println("iWord HTTP API server starting on :8765")
	fmt.Println("  GET  /seek?word=<word>")
	fmt.Println("  POST /map  {\"text\": \"...\", \"mode\": 3}")

	http.HandleFunc("/seek", seekHandler)
	http.HandleFunc("/map", mapHandler)
	if err := http.ListenAndServe(":8765", nil); err != nil {
		fmt.Fprintf(http.ResponseWriter(nil), "error: %v", err)
	}
}
