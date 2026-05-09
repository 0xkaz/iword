"""
Example: Using iWord as a pre-processing filter in a RAG pipeline.

Scenario: Before embedding documents into a vector store,
filter out spam/forbidden words and mask sensitive terms.

Requires:
  - iword.so built: make pecl
  - Dictionary loaded: bin/iwordctl load your_dict.txt
"""
import iword


def preprocess_for_rag(documents: list[str]) -> list[str]:
    """
    Clean documents before embedding:
    1. Filter out documents dominated by spam keywords
    2. Mask forbidden words in remaining documents
    """
    clean = []
    for doc in documents:
        matches = iword.map(doc, iword.MODE_HTML | iword.MODE_FORBID)
        if not matches:
            clean.append(doc)
            continue

        spam = [m for m in matches if m.key == iword.KEY_SPAM]
        spam_ratio = sum(m.length for m in spam) / max(len(doc), 1)

        if spam_ratio > 0.1:
            print(f"[skip] spam ratio {spam_ratio:.1%}: {doc[:60]}...")
            continue

        clean.append(iword.filter_text(doc, iword.MODE_HTML | iword.MODE_FORBID))

    return clean


def route_to_agent(text: str) -> str:
    """
    Route text to an agent based on keyword category.
    Category keys are defined in the dictionary:
      key=3 → legal terms → legal agent
      key=4 → technical terms → tech agent
      key=9 → general → default agent
    """
    matches = iword.map(text, iword.MODE_ENGLISH)
    keys = {m.key for m in matches}

    if 3 in keys:
        return "legal_agent"
    if 4 in keys:
        return "tech_agent"
    return "default_agent"


if __name__ == "__main__":
    docs = [
        "This is a normal document about machine learning.",
        "Buy cheap spam spam spam click here now!!!",
        "<html><body>Web content with <b>keywords</b></body></html>",
    ]

    print("=== RAG pre-processing ===")
    result = preprocess_for_rag(docs)
    for i, doc in enumerate(result):
        print(f"[{i}] {doc[:80]}")

    print("\n=== Agent routing ===")
    queries = [
        "What is the liability clause in this contract?",
        "How do I fix the segmentation fault in my C code?",
        "Tell me about the weather today.",
    ]
    for q in queries:
        agent = route_to_agent(q)
        print(f"  '{q[:50]}' → {agent}")
