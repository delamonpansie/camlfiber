let main () =
  let mutex = Fiber.Mutex.create () in
  let lock_sleep_and_print msg =
    Fiber.Mutex.lock mutex;
    String.iter (fun c ->
        Fiber.sleep 0.01;
        print_char c) msg;
    Fiber.Mutex.unlock mutex
  in
  let fs = List.map (Fiber.create lock_sleep_and_print)
             ["hello, "; "world\n"] in
  List.iter Fiber.resume fs;
  List.iter Fiber.join fs


let _ = Fiber.run main ()
