
-- Test different src functions

local tests = {
   makeplural = {
      algae = "algae",
      amoeba = "amoebae",
      baluchitherium = "baluchitheria",
      bordeau = "bordeaus",
      ["bunch of grapes"] = "bunches of grapes",
      bus = "buses",
      candelabrum = "candelabra",
      caveman = "cavemen",
      child = "children",
      cookie = "cookies",
      deer = "deer",
      dingo = "dingoes",
      epoch = "epochs",
      fish = "fish",
      foot = "feet",
      fox = "foxes",
      fungus = "fungi",
      gateau = "gateaus",
      gateaux = "gateauxes",
      gauntlet = "gauntlets",
      ["gauntlet of power"] = "gauntlets of power",
      goose = "geese",
      homunculus = "homunculi",
      hoof = "hooves",
      hyphae = "hyphae",
      knife = "knives",
      larvae = "larvae",
      loch = "lochs",
      lotus = "lotuses",
      louse = "lice",
      manes = "manes",
      matzah = "matzot",
      matzoh = "matzot",
      mech = "mechs",
      mouse = "mice",
      mycelium = "mycelia",
      nemesis = "nemeses",
      nerf = "nerfs",
      ninja = "ninja",
      ox = "oxen",
      potato = "potatoes",
      priestess = "priestesses",
      ronin = "ronin",
      roshi = "roshi",
      scale = "scales",
      serf = "serfs",
      shaman = "shamans",
      sheep = "sheep",
      shito = "shito",
      ["slice of cake"] = "slices of cake",
      stamen = "stamens",
      tech = "techs",
      tengu = "tengu",
      tomato = "tomatoes",
      tooth = "teeth",
      tuna = "tuna",
      valkyrie = "valkyries",
      VAX = "VAXES",
      vertebra = "vertebrae",
      vortex = "vortices",
      woman = "women",
      wumpus = "wumpuses",
      zorkmid = "zorkmids",
   },
   an = {
      a = "an a",
      b = "a b",
      ["the foo"] = "the foo",
      ["molten lava"] = "molten lava",
      ["iron bars"] = "iron bars",
      ice = "ice",
      unicorn = "a unicorn",
      uranium = "a uranium",
      ["one-eyed"] = "a one-eyed",
      candy = "a candy",
      eucalyptus = "a eucalyptus",
   }
}

for func, fval in pairs(tests) do
   for instr, outstr in pairs(fval) do
      local ret = nh[func](instr)
      if ret ~= outstr then
         error(func .. "(\"" .. instr .. "\") != \"" .. outstr .. "\" (returned \"" .. ret .. "\") instead")
      end
   end
end
