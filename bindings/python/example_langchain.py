"""
iWord integration examples for LangChain and LlamaIndex.

Prerequisites:
    pip install langchain langchain-openai llama-index-core
    make && bin/iwordctl load /path/to/words.txt

This file shows three integration patterns:
  1. LangChain Tool  — use iWord as a callable tool in an agent
  2. LangChain runnable guardrail — pre-filter inputs in an LCEL chain
  3. LlamaIndex NodePostprocessor — filter retrieved nodes before LLM synthesis
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from iword import seek, map as iword_map, filter_text, MODE_HTML, MODE_FORBID

# ---------------------------------------------------------------------------
# Pattern 1: LangChain Tool
# ---------------------------------------------------------------------------
try:
    from langchain.tools import tool
    from langchain_openai import ChatOpenAI
    from langchain.agents import create_tool_calling_agent, AgentExecutor
    from langchain_core.prompts import ChatPromptTemplate

    @tool
    def iword_seek_tool(word: str) -> str:
        """Check whether a word is in the iWord dictionary and return its category key.
        Returns -1 if not found, otherwise the category key (0=hidden,1=adult,2=spam,9=default)."""
        key = seek(word)
        if key == -1:
            return f"'{word}' not found in dictionary"
        categories = {0: "hidden", 1: "adult", 2: "spam", 9: "word"}
        label = categories.get(key, f"key={key}")
        return f"'{word}' found, category: {label}"

    @tool
    def iword_filter_tool(text: str) -> str:
        """Filter harmful words from text using iWord. Returns censored text."""
        return filter_text(text, MODE_HTML | MODE_FORBID)

    def build_langchain_agent():
        llm = ChatOpenAI(model="gpt-4o-mini", temperature=0)
        tools = [iword_seek_tool, iword_filter_tool]
        prompt = ChatPromptTemplate.from_messages([
            ("system", "You are a content moderation assistant."),
            ("human", "{input}"),
            ("placeholder", "{agent_scratchpad}"),
        ])
        agent = create_tool_calling_agent(llm, tools, prompt)
        return AgentExecutor(agent=agent, tools=tools, verbose=True)

    # Usage:
    # agent = build_langchain_agent()
    # agent.invoke({"input": "Is 'spam_word' in the filter list?"})

except ImportError:
    pass  # langchain not installed


# ---------------------------------------------------------------------------
# Pattern 2: LangChain LCEL guardrail (input pre-filter)
# ---------------------------------------------------------------------------
try:
    from langchain_core.runnables import RunnableLambda, RunnablePassthrough
    from langchain_core.output_parsers import StrOutputParser
    from langchain_openai import ChatOpenAI
    from langchain_core.prompts import ChatPromptTemplate

    class IWordGuardrail:
        """Pre-filter runnable: blocks requests containing forbidden keywords."""

        def __init__(self, forbidden_key: int = 2, mode: int = MODE_HTML | MODE_FORBID):
            self.forbidden_key = forbidden_key
            self.mode = mode

        def __call__(self, inputs: dict) -> dict:
            text = inputs.get("question", "")
            matches = iword_map(text, self.mode)
            blocked = [m for m in matches if m.key == self.forbidden_key]
            if blocked:
                raise ValueError(
                    f"Input blocked: contains {len(blocked)} forbidden term(s). "
                    "Please rephrase your question."
                )
            return inputs

    def build_guarded_chain():
        guardrail = IWordGuardrail(forbidden_key=2)
        llm = ChatOpenAI(model="gpt-4o-mini")
        prompt = ChatPromptTemplate.from_template("Answer the question: {question}")

        chain = (
            RunnableLambda(guardrail)
            | prompt
            | llm
            | StrOutputParser()
        )
        return chain

    # Usage:
    # chain = build_guarded_chain()
    # try:
    #     result = chain.invoke({"question": "How do I buy spam_product?"})
    # except ValueError as e:
    #     print(f"Blocked: {e}")

except ImportError:
    pass  # langchain not installed


# ---------------------------------------------------------------------------
# Pattern 3: LangChain router — zero-latency keyword-based routing
# ---------------------------------------------------------------------------
try:
    from langchain_core.runnables import RunnableLambda
    from langchain_openai import ChatOpenAI
    from langchain_core.prompts import ChatPromptTemplate
    from langchain_core.output_parsers import StrOutputParser

    ROUTE_DICT = {
        # iWord category key → specialized chain name
        # Design your dictionary so words carry domain-specific keys
        3: "medical",
        4: "legal",
        5: "finance",
        9: "general",
    }

    def iword_router(inputs: dict) -> str:
        text = inputs.get("question", "")
        matches = iword_map(text, MODE_HTML)
        if matches:
            # Route based on the most-frequent key
            from collections import Counter
            key = Counter(m.key for m in matches).most_common(1)[0][0]
            return ROUTE_DICT.get(key, "general")
        return "general"

    def build_routing_chain():
        llm = ChatOpenAI(model="gpt-4o-mini")
        general_chain = ChatPromptTemplate.from_template("Answer: {question}") | llm | StrOutputParser()
        medical_chain = ChatPromptTemplate.from_template("As a medical expert, answer: {question}") | llm | StrOutputParser()

        def route(inputs):
            destination = iword_router(inputs)
            if destination == "medical":
                return medical_chain
            return general_chain

        return RunnableLambda(route)

except ImportError:
    pass  # langchain not installed


# ---------------------------------------------------------------------------
# Pattern 4: LlamaIndex NodePostprocessor
# ---------------------------------------------------------------------------
try:
    from llama_index.core.postprocessor.types import BaseNodePostprocessor
    from llama_index.core.schema import NodeWithScore, QueryBundle
    from typing import List, Optional

    class IWordPostprocessor(BaseNodePostprocessor):
        """Remove retrieved nodes that contain forbidden keywords before LLM synthesis.

        Useful when your vector index contains mixed content and you want to
        ensure the LLM never sees adult/spam content even if it scores well.
        """

        forbidden_keys: List[int] = [1, 2]  # adult=1, spam=2
        mode: int = MODE_HTML | MODE_FORBID

        def _postprocess_nodes(
            self,
            nodes: List[NodeWithScore],
            query_bundle: Optional[QueryBundle] = None,
        ) -> List[NodeWithScore]:
            filtered = []
            for node_with_score in nodes:
                text = node_with_score.node.get_content()
                matches = iword_map(text, self.mode)
                forbidden = [m for m in matches if m.key in self.forbidden_keys]
                if not forbidden:
                    filtered.append(node_with_score)
            return filtered

    # Usage with LlamaIndex query engine:
    #
    # from llama_index.core import VectorStoreIndex
    # index = VectorStoreIndex.from_documents(documents)
    # query_engine = index.as_query_engine(
    #     node_postprocessors=[IWordPostprocessor()]
    # )
    # response = query_engine.query("What is the refund policy?")

except ImportError:
    pass  # llama-index not installed


# ---------------------------------------------------------------------------
# Standalone demo (no LLM required)
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    print("iWord + LangChain/LlamaIndex integration demo")
    print("=" * 50)

    test_texts = [
        "This is a normal question about Python.",
        "How do I report spam to the authorities?",
        "Tell me about adult education programs.",
    ]

    for text in test_texts:
        matches = iword_map(text, MODE_HTML | MODE_FORBID)
        filtered = filter_text(text, MODE_HTML | MODE_FORBID)
        print(f"\nInput : {text}")
        print(f"Matches: {len(matches)} keywords found")
        print(f"Filtered: {filtered}")
