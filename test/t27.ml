let main () =
  let a = Fiber.create (fun () -> print_string "ok";
				  print_newline ()) () in
  print_string "before_wake";
  print_newline ();
  Fiber.wake a;
  Fiber.cancel_wake a;
  print_string "after_wake";
  print_newline ();
  Fiber.resume a

let _ = Fiber.run main ()
