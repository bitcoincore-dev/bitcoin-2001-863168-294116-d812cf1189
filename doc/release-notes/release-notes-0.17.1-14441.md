`listtransactions` label support
--------------------------------

The `listtransactions` RPC `account` parameter which was deprecated in 0.17.0
and renamed to `dummy` has been un-deprecated and renamed again to `label`.

When bitcoin is configured with the `-deprecatedrpc=accounts` setting,
specifying a `label`/`account`/`dummy` argument will return both outgoing and
incoming transactions. Without the `-deprecatedrpc=accounts` setting, it will
only return incoming transactions (because it used to be possible to create
transactions spending from specific accounts, but this is no longer possible
with labels).

Also for backwards compatibility when `-deprecatedrpc=accounts` is set, it's
possible to pass an empty string label `""` to list transactions that aren't
labeled. Without `-deprecatedrpc=accounts`, passing the empty string is an
error because returning only non-labeled transactions isn't actually very
useful, and can be confusing.
