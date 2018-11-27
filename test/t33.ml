
let main () =
  let f = Fiber.create (fun () ->
	      Fiber.sleep 0.01; (* long io wait *)
	      "ok") () in
  Fiber.resume f;
  print_string (Fiber.join f); (* join first time *)
  print_newline ();
  print_string (Fiber.join f); (* join second time *)
  print_newline ()

let _ = Fiber.run main ()

