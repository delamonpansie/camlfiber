let main () =
  let mutex = Fiber.Mutex.create () in
  let c = Fiber.Condition.create () in
  Fiber.Condition.signal c; (* it's ok to signal a condition that nobody waits *)
  Fiber.create (fun () ->
      Fiber.Mutex.lock mutex;
      Fiber.Condition.wait ~mutex c;
      assert (Fiber.Mutex.is_locked mutex);
      print_string "ok";
      print_newline ()) () |> Fiber.resume;
  assert (Fiber.Mutex.is_locked mutex = false); (* mutex is unlocked by Condition.wait *)
  Fiber.Condition.signal c

let _ = Fiber.run main ()
