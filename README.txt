Download https://www.gnu.org/software/global/

set GTAGS dir environment
ex:
export GTAGS_DIR=/home/tmp/global-6.6.10/

cd tree-sitter-gtags-parser
make

update gtags.conf with the below details

treesitter:\
       :tc=tree-sitter:tc=htags:

# Plug-in parser to use tree-sitter
tree-sitter|settings to use tree-sitter as plug-in parser:\
       :tc=common:\
       :langmap=c\:.c.h:\
       :gtags_parser=c\:tree-sitter-gtags-parser-path/tree-sitter-query.so:


export the path to tree-sitter grammer libraries like  libtree-sitter-c.so
export GTAGS_TREE_SITTER_LANG_PATH=/path-to-tree-sitter/

Also export the path to the language queries directories. (C language query is part of this repo)
export GTAGS_TREE_SITTER_QUERY_PATH=/home/opensource/sources/tree-sitter-gtags-parser/queries/

run gtags
gtags -v --gtagsconf /path-to-updated/gtags.conf --gtagslabel treesitter

or

export GTAGSLABEL=treesitter
export GTAGSCONF=/tmp/global-6.6.10/gtags.conf
export GTAGS_TREE_SITTER_LANG_PATH=/tmp/tree-sitter/
export GTAGS_TREE_SITTER_QUERY_PATH=/tmp/tree-sitter-gtags-parser/queries/

gtags -v
