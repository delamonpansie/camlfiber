let fb = Fiber.create (fun () ->
	     for i = 1 to 10 do
	       print_int i;
	       print_newline ();
	       Fiber.yield ()
	     done) ()

let _ =
  for j = 1 to 10 do
    Fiber.resume fb
  done
