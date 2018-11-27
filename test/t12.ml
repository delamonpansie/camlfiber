let mutex = Fiber.Mutex.create ()
let _ =
  assert(Fiber.Mutex.is_locked mutex = false);
  Fiber.Mutex.lock mutex;
  assert(Fiber.Mutex.is_locked mutex = true);
  Fiber.Mutex.unlock mutex;
  assert(Fiber.Mutex.is_locked mutex = false);
