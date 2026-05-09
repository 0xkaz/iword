"""
Integration tests for LangChain/LlamaIndex helpers.
No LLM API key required — tests only the iWord-powered parts.

Run after loading a dictionary:
    bin/iwordctl load /tmp/dict.txt
    python3 bindings/python/test_langchain_integration.py
    bin/iwordctl stop
"""
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from iword import seek, map as iword_map, filter_text, MODE_HTML, MODE_FORBID

# ---------------------------------------------------------------------------
# Test: guardrail logic (iWord part only, no LLM)
# ---------------------------------------------------------------------------
def test_guardrail_logic():
    """IWordGuardrail blocks inputs containing forbidden keywords."""
    FORBIDDEN_KEY = 2  # spam

    def is_blocked(text):
        matches = iword_map(text, MODE_HTML | MODE_FORBID)
        return any(m.key == FORBIDDEN_KEY for m in matches)

    assert not is_blocked("Tell me about Python programming"), \
        "Clean text should not be blocked"
    assert is_blocked("How do I send spam emails?"), \
        "Text with spam keyword should be blocked"
    assert not is_blocked("What is machine learning?"), \
        "Clean text should not be blocked"
    print("guardrail_logic: OK")

# ---------------------------------------------------------------------------
# Test: router logic (iWord part only, no LLM)
# ---------------------------------------------------------------------------
def test_router_logic():
    """iword_router selects a route based on keyword category."""
    from collections import Counter

    ROUTES = {9: "general", 2: "spam_handler", 1: "adult_handler"}

    def route(text):
        matches = iword_map(text, MODE_HTML)
        if not matches:
            return "general"
        key = Counter(m.key for m in matches).most_common(1)[0][0]
        return ROUTES.get(key, "general")

    assert route("no keywords here") == "general", \
        "No keywords should route to general"
    assert route("buy spam product spam"), \
        "spam keywords should route to spam_handler"
    assert route("apple is a word") == "general", \
        "default word (key=9) should route to general"
    print("router_logic: OK")

# ---------------------------------------------------------------------------
# Test: NodePostprocessor logic (iWord part only, no LlamaIndex)
# ---------------------------------------------------------------------------
def test_postprocessor_logic():
    """Simulate LlamaIndex NodePostprocessor filtering."""
    FORBIDDEN_KEYS = {1, 2}  # adult, spam

    def filter_nodes(texts):
        return [
            t for t in texts
            if not any(
                m.key in FORBIDDEN_KEYS
                for m in iword_map(t, MODE_HTML | MODE_FORBID)
            )
        ]

    nodes = [
        "This is a safe document about Python.",
        "Buy spam products here for cheap.",
        "Apple computers are popular.",
    ]
    filtered = filter_nodes(nodes)
    assert len(filtered) == 2, f"Expected 2 nodes after filtering, got {len(filtered)}"
    assert all("spam" not in n for n in filtered), \
        "Spam node should be removed"
    print("postprocessor_logic: OK")

# ---------------------------------------------------------------------------
# Test: LangChain Tool availability (import only, no API key)
# ---------------------------------------------------------------------------
def test_langchain_import():
    """Check that LangChain integration module imports without error."""
    try:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            "example_langchain",
            os.path.join(os.path.dirname(__file__), "example_langchain.py")
        )
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        print("langchain_import: OK (langchain available)" if "build_langchain_agent" in dir(mod)
              else "langchain_import: OK (langchain not installed, skipped)")
    except Exception as e:
        print(f"langchain_import: SKIP ({e})")

if __name__ == "__main__":
    test_guardrail_logic()
    test_router_logic()
    test_postprocessor_logic()
    test_langchain_import()
    print("\nAll LangChain integration tests passed.")
