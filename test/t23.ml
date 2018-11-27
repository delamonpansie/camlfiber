module S = Set.Make(String)

let n = 50
let main () =
  let mvar = Fiber.MVar.create S.empty in

  let rand_string n =
    let alphanum =  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" in
    let len = String.length alphanum in
    Bytes.init n (fun _ -> alphanum.[Random.int len]) |> Bytes.to_string
  in
  let worker () =
    for i = 1 to n do
      let s = Fiber.MVar.take mvar in
      let el = rand_string (Random.int 1024) in
      Fiber.MVar.put mvar (S.add el s);
      Fiber.yield ()
    done
  in
  let zombies = Array.init 128 (fun _ -> Fiber.create Fiber.yield ()) in
  let w = Array.init 4096 (fun _ -> Fiber.create worker ()) in
  Array.iter Fiber.resume zombies;

  Array.iter Fiber.resume zombies;
  Array.iter Fiber.resume w;

  for i = 1 to n do
    Array.iter Fiber.resume w;
    if i mod 20 == 0 then
      Gc.compact ()
  done;
  Fiber.MVar.take mvar


let _ =
  (match Fiber.run main ()
   with Some s -> print_int (S.cardinal s);
		  print_newline ();
      | None -> assert false);
  let stat = Gc.stat () in
  assert(stat.minor_collections > 0);
  assert(stat.major_collections > 0);
  assert(stat.compactions > 0)
