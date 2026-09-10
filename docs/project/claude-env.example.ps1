# Copy this file outside the repository before use and provide credentials
# through a local secret store or the current shell session.
$env:ANTHROPIC_BASE_URL = "https://api.deepseek.com/anthropic"
$env:ANTHROPIC_AUTH_TOKEN = "<set-locally-do-not-commit>"
$env:ANTHROPIC_MODEL = "deepseek-v4-pro[1m]"
$env:ANTHROPIC_DEFAULT_OPUS_MODEL = "deepseek-v4-pro[1m]"
$env:ANTHROPIC_DEFAULT_SONNET_MODEL = "deepseek-v4-pro[1m]"
$env:ANTHROPIC_DEFAULT_HAIKU_MODEL = "deepseek-v4-flash"
$env:CLAUDE_CODE_SUBAGENT_MODEL = "deepseek-v4-flash"
$env:CLAUDE_CODE_EFFORT_LEVEL = "max"
$env:CLAUDE_CODE_AUTO_COMPACT_WINDOW = "786432"
