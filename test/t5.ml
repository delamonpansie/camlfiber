
let f = Fiber.create (fun () -> Fiber.sleep 0.01; "ok\n") ()

let main () =
  Fiber.resume f;
  print_string (Fiber.join f);
  "ret\n"

let _ =
  match Fiber.run main ()
  with Some str -> print_string str
     | None -> assert false
