let main () =
  let c = Fiber.Condition.create () in
  let wait msg =
    Fiber.Condition.wait c;
    print_string msg;
    print_newline ()
  in
  let fs = List.map (Fiber.create wait) ["1st"; "2nd"; "3rd"] in
  List.iter Fiber.resume fs;
  Fiber.Condition.broadcast c;
  List.iter Fiber.join fs;
  print_string "done\n"

let _ = Fiber.run main ()
