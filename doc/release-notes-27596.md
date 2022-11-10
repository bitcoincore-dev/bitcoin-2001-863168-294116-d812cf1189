ZMQ
---

- When an assumeutxo snapshot is in use, block connect events are only sent
  for the active (snapshot) chainstate. Notifications will not be sent for
  historical blocks beneath the snapshot base which are being validated in
  the background.
