let main () =
  Fiber.create (fun () -> Fiber.sleep 0.01;
			  print_string "fiber";
			  print_newline ();
			  Fiber.break ()) () |> Fiber.resume;
  Fiber.sleep 1.0;
  print_string "not printed";
  print_newline ()

let _ =
  match Fiber.run main ()
  with None -> print_string "exit";
	       print_newline ()
     | Some _ -> assert false
