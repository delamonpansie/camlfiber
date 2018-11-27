let main () =
  let v = Fiber.MVar.create_empty () in
  Fiber.create (fun () ->
    assert(Fiber.MVar.is_empty v);
    Fiber.MVar.take v |> print_string) () |> Fiber.resume;
  Fiber.MVar.put v "ok\n"

let _ = Fiber.run main ()
