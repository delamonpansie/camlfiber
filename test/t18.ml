let main () =
  let v = Fiber.MVar.create "ok" in
  assert(Fiber.MVar.is_empty v = false);
  Fiber.MVar.take v |> print_string;
  print_newline ();
  assert(Fiber.MVar.is_empty v)

let _ = Fiber.run main ()
