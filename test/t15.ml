let main () =
  let mutex = Fiber.Mutex.create () in
  let f = Fiber.create (fun () ->
              Fiber.Mutex.with_lock mutex (fun () ->
                  Fiber.sleep 0.01)) ()
  in
  Fiber.resume f;
  assert (Fiber.Mutex.is_locked mutex = true);
  Fiber.join f;
  assert (Fiber.Mutex.is_locked mutex = false)

let _ = Fiber.run main ()
