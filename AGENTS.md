# ProjectNight Agent Notes

## Network Optimization TODO

Scope: focus on the `NightNetwork` transport layer and server/client network runtime. Do not include packet schema, protocol content handling, or gameplay/content logic here.

- [x] Fix shutdown ordering in `NightNetwork::Server` and `NightNetwork::Client`.
  - Join/stop I/O threads before draining lock-free queues or destroying queued objects.
  - Add an explicit internal shutdown path so callbacks cannot push into queues while destructors are cleaning them up.
  - Implemented explicit shutdown guards and shutdown paths in `NightNetwork/src/Server.cpp` and `NightNetwork/src/Client.cpp`.

- [x] Add outbound backpressure for per-session and client write queues.
  - Track queued frame count or queued bytes.
  - Define a policy for overflow, preferably disconnecting slow server-side clients.
  - Avoid unbounded growth of `Session::write_queue_` and `Client::Impl::write_queue`.
  - Implemented a 1024-frame pending write cap; overflow closes the connection.

- [x] Add `tcp::no_delay(true)` after successful accept/connect.
  - Apply to server accepted sockets and the client socket.
  - Consider making it configurable if bulk-transfer behavior is needed later.
  - Applied to server accepted sockets and client connected sockets.

- [ ] Reduce hot-path heap allocation in receive queues.
  - Current server queue allocates `Packet` with `new` per event.
  - Current client queue allocates `std::vector<uint8_t>` with `new` per received payload.
  - Consider packet object pooling, fixed ring slots, or queueing move-friendly value storage.

- [ ] Remove the extra receive payload copy where practical.
  - Current flow reads into `body_buf_`, then copies into a pooled vector.
  - Prefer acquiring a payload buffer after header decode and reading directly into that buffer.
  - Move the completed buffer into the game-thread queue.

- [ ] Rework broadcast enqueue cost for large session counts.
  - Current `Server::broadcast()` clones the frame for every session on `server_strand`.
  - Consider immutable shared frame storage or preparing clone work outside the server strand.
  - Keep `server_strand` work short so accept/session map operations are not delayed.

- [ ] Make server I/O thread count configurable.
  - Current default uses `std::thread::hardware_concurrency()`.
  - Keep that as the default, but expose an option for small servers, tests, and production tuning.

- [ ] Add focused network runtime tests or stress harnesses.
  - Cover clean shutdown with active reads/writes.
  - Cover slow receiver backpressure.
  - Cover burst receive behavior and dropped packet accounting.
  - Cover many-session broadcast latency.

## Chat Persistence TODO

Scope: add MySQL-backed persistence for chat-related server data. Keep database code out of the `NightNetwork` transport layer; integrate from `NightServer` or a dedicated persistence module.

- [ ] Choose and wire a MySQL client dependency.
  - Prefer a maintained C++ connector available through vcpkg if practical.
  - Keep database connection setup configurable through environment variables or config files, not hardcoded credentials.

- [ ] Add a small persistence boundary for chat storage.
  - Define a repository/service interface for saving chat messages.
  - Keep protocol parsing and gameplay/session logic separate from SQL details.
  - Make database failures observable without crashing the server loop.

- [ ] Design the initial chat schema.
  - Store message id, room/channel id, sender/user/session identity, message text, and created timestamp.
  - Add indexes for room/channel history queries and recent-message loading.
  - Include a migration or schema bootstrap path.

- [ ] Save incoming chat messages from the server path.
  - Persist after validation and before/around broadcast according to desired delivery semantics.
  - Decide whether failed persistence should block broadcast, drop the message, or broadcast with retry/logging.
  - Avoid blocking the main game/server tick on synchronous database writes.

- [ ] Add chat history loading if needed.
  - Define how many recent messages to load on room join or client connect.
  - Keep response size bounded.

- [ ] Add focused persistence tests or a local integration harness.
  - Cover repository SQL mapping.
  - Cover save failure behavior.
  - Cover message ordering and timestamp handling.

## Verification Notes

- Plain `cmake --build out\build\x64-debug` may fail from a regular shell if the MSVC developer environment is not loaded; observed failures included missing standard headers such as `cstdint` and `chrono`.
- Running through `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat` succeeded:
  - `cmake --build out\build\x64-debug`
  - `ctest --test-dir out\build\x64-debug --output-on-failure`
