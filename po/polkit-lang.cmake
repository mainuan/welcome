# msgfmt --xml writes BCP 47 tags (xml:lang="pt-BR"); polkit looks translations
# up by POSIX locale name (pt_BR, then pt), as the original policy file did.
# Usage: cmake -DFILE=<policy> -P polkit-lang.cmake
file(READ "${FILE}" content)
string(REGEX REPLACE "xml:lang=\"([a-z]+)-([A-Za-z0-9]+)\"" "xml:lang=\"\\1_\\2\"" content "${content}")
file(WRITE "${FILE}" "${content}")
