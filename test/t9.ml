let main () =
  let mutex = Fiber.Mutex.create () in
  let c = Fiber.Condition.create () in
  let shared = ref 3 in
  let wait msg =
    Fiber.Mutex.lock mutex;
    while !shared > 0 do
      Fiber.Condition.wait ~mutex c;
      assert(Fiber.Mutex.is_locked mutex);
      print_string (msg ^ " ");
      print_int !shared;
      print_newline ();
    done;
    Fiber.Mutex.unlock mutex;
  in
  let fs = List.map (Fiber.create wait) ["1st"; "2nd"; "3rd"] in
  List.iter Fiber.resume fs;

  for i = 1 to 3 do
    Fiber.Mutex.with_lock mutex (fun () ->
        decr shared;
        Fiber.Condition.broadcast c);
    Fiber.sleep 0.01
  done;

  List.iter Fiber.join fs

let _ = Fiber.run main ()
