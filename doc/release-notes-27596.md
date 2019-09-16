Pruning
-------

When using assumeutxo with `-prune`, the prune budget may be exceeded. Prune budget is
normally split evenly across each chainstate, unless the resulting prune budget per
chainstate is beneath `MIN_DISK_SPACE_FOR_BLOCK_FILES` in which case that value will be
used.
