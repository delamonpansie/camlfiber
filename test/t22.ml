let main () =
  Fiber.create (fun () -> Fiber.sleep 1.0;
			  print_string "fiber";
			  print_newline ()) () |> Fiber.resume;
  print_string "loop";
  print_newline ()

let _ =
  Unix.alarm 1 |> ignore;
  Fiber.run main () |> ignore;
  print_string "exit";
  print_newline ()
