let main () =
  let a = Fiber.create (fun () ->
	      let v = Fiber.unsafe_yield () in
	      print_string v;
	      print_newline ()) () in
  Fiber.resume a; (* resume [a] so it can suspend on yield *)
  Fiber.create (fun () ->
      Fiber.unsafe_resume a "hello, world") ()
  |> Fiber.resume

let _ = Fiber.run main ()
