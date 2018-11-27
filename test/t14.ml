let _ =
  let mutex = Fiber.Mutex.create () in
  begin
    try
      Fiber.Mutex.with_lock mutex (fun _ -> raise Exit)
    with
      Exit -> ();
  end;
  assert (Fiber.Mutex.is_locked mutex = false)
