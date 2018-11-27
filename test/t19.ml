let main () =
  let v = Fiber.MVar.create "1st" in
  assert(Fiber.MVar.is_empty v = false);
  Fiber.create (fun () ->
    Fiber.MVar.put v "2nd") () |> Fiber.resume;
  for _ = 1 to 2 do
    Fiber.MVar.take v |> print_string;
    print_newline ();
  done;
  assert(Fiber.MVar.is_empty v)

let _ = Fiber.run main ()
