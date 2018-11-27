let mutex = Fiber.Mutex.create ()

let _ =
  assert(Fiber.Mutex.try_lock mutex == true);
  assert(Fiber.Mutex.try_lock mutex == false)
