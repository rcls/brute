
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;
entity delay is
  generic (na : integer; nb : integer);

  port (clk: in std_logic;
        Da: in std_logic_vector (31 downto 0);
        Qa: out std_logic_vector (31 downto 0);
        Db: in std_logic_vector (31 downto 0);
        Qb: out std_logic_vector (31 downto 0));
end delay;

architecture Behavioral of delay is
  component mem6432 IS
    port (
      clka: in std_logic;
      dina: in std_logic_vector (31 downto 0);
      addra: in std_logic_vector (5 downto 0);
      wea: in std_logic_vector (0 downto 0);
      douta: out std_logic_vector (31 downto 0);
      
      clkb: in std_logic;
      dinb: in std_logic_vector (31 downto 0);
      addrb: in std_logic_vector (5 downto 0);
      web: in std_logic_vector (0 downto 0);
      doutb: out std_logic_vector (31 downto 0));
  end component;

  signal counta : std_logic_vector (4 downto 0);
  signal countb : std_logic_vector (4 downto 0);
  signal addra : std_logic_vector (5 downto 0);
  signal addrb : std_logic_vector (5 downto 0);
begin
  addra <= '0' & counta;
  addrb <= '1' & countb;
  mem : mem6432 port map (clka => clk,
                          dina => Da, douta => Qa,
                          wea => "1",
                          addra => addra,
                          
                          clkb => clk,
                          dinb => Db, doutb => Qb,
                          web => "1",
                          addrb => addrb);
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      if counta = conv_std_logic_vector (na-2, 5) then
        counta <= "00000";
      else
        counta <= counta + 1;
      end if;
    end if;
    if Clk'event and Clk = '1' then
      if countb = conv_std_logic_vector (nb-2, 5) then
        countb <= "00000";
      else
        countb <= countb + 1;
      end if;
    end if;
  end process;
end Behavioral;
  
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;


entity md5 is
  Port (xxIN : in  std_logic_vector (31 downto 0);
        Aout : out std_logic_vector (31 downto 0);
        Bout : out std_logic_vector (31 downto 0);
        Cout : out std_logic_vector (31 downto 0);
        Dout : out std_logic_vector (31 downto 0);
        Clk : in std_logic;
        phaseA : in std_logic;
        phaseB : in std_logic);
end md5;

--#define F(x, y, z) OR (AND ((x), (y)), ANDN ((x), (z)))
--#define G(x, y, z) OR (AND ((z), (x)), ANDN ((z), (y)))
--#define H(x, y, z) XOR ((x), XOR ((y), (z)))
--#define I(x, y, z) XOR ((y), OR ((x), NOT ((z))))

architecture Behavioral of md5 is
  subtype word is std_logic_vector (31 downto 0);
  
  type dataset is array (natural range <>) of word;

  type iarray is array (natural range <>) of integer;
  
  constant S1 : iarray (0 to 3) := (7, 12, 17, 22);
  constant S2 : iarray (0 to 3) := (5, 9, 14, 20);
  constant S3 : iarray (0 to 3) := (4, 11, 16, 23);
  constant S4 : iarray (0 to 3) := (6, 10, 15, 21);

  constant phaseA8 : std_logic_vector(7 downto 0) :=
    (phaseA, phaseA, phaseA, phaseA,
     phaseA, phaseA, phaseA, phaseA);
  constant phaseA32 : word := phaseA8 & phaseA8 & phaseA8 & phaseA8;
  constant phaseB8 : std_logic_vector(7 downto 0) :=
    (phaseB, phaseB, phaseB, phaseB,
     phaseB, phaseB, phaseB, phaseB);
  constant phaseB32 : word := phaseB8 & phaseB8 & phaseB8 & phaseB8;
  
  constant KK : dataset (0 to 63) := (
    x"d76aa478", x"e8c7b756", x"242070db", x"c1bdceee",
    x"f57c0faf", x"4787c62a", x"a8304613", x"fd469501",
    x"698098d8", x"8b44f7af", x"ffff5bb1", x"895cd7be",
    x"6b901122", x"fd987193", x"a679438e", x"49b40821",
    x"f61e2562", x"c040b340", x"265e5a51", x"e9b6c7aa",
    x"d62f105d", x"02441453", x"d8a1e681", x"e7d3fbc8",
    x"21e1cde6", x"c33707d6", x"f4d50d87", x"455a14ed",
    x"a9e3e905", x"fcefa3f8", x"676f02d9", x"8d2a4c8a",
    x"fffa3942", x"8771f681", x"6d9d6122", x"fde5380c",
    x"a4beea44", x"4bdecfa9", x"f6bb4b60", x"bebfbc70",
    x"289b7ec6", x"eaa127fa", x"d4ef3085", x"04881d05",
    x"d9d4d039", x"e6db99e5", x"1fa27cf8", x"c4ac5665",
    x"f4292244", x"432aff97", x"ab9423a7", x"fc93a039",
    x"655b59c3", x"8f0ccc92", x"ffeff47d", x"85845dd1",
    x"6fa87e4f", x"fe2ce6e0", x"a3014314", x"4e0811a1",
    x"f7537e82", x"bd3af235", x"2ad7d2bb", x"eb86d391");
  
--alias u32 is std_logic_vector;

  function phased (x : word; y : word) return word is
  begin
    return (x and y) or (phaseA32 and x and not y)
      or (phaseB32 and y and not x);
  end phased;

  function phasedCV (x : word;
                     y : word)
    return std_logic_vector is
  begin
    return ((phaseA32 or y) and x)
      or   (phaseB32 and y and not x);
  end phasedCV;

  function phasedVC (x : word;
                     y : word)
    return std_logic_vector is
  begin
    return ((phaseA32 or x) and y)
      or   (phaseB32 and x and not y);
  end phasedVC;

  function phasedVVa (x : word;
                      y : word)
    return std_logic_vector is
  begin
    return (x and phaseA32) or (y and not phaseA32);
  end phasedVVa;

  function phasedVVb (x : word;
                      y : word)
    return std_logic_vector is
  begin
    return (x and not phaseB32) or (y and phaseB32);
  end phasedVVb;

  
  function F(x : word; y : word; z : word)
    return word is
  begin
    return (x and y) or (x and not z);
  end F;

  function G(x : word; y : word; z : word)
    return word is
  begin
    return (z and x) or (z and not y);
  end G;

  function H(x : word; y : word; z : word)
    return word is
  begin
    return x xor y xor z;
  end H;

  function I(x : word; y : word; z : word)
    return word is
  begin
    return y xor (x or not z);
  end I;

--#define GENERATE(FUN,a, b, c, d, x, s, ac) {		\
--	(a) = A4 ((a), FUN ((b), (c), (d)), (x), DIAG (ac));	\
--	(a) = ROTATE_LEFT ((a), (s));			\
--        (a) = ADD ((a), (b));				\
--    }
  function FF(a : word;
              b : word;
              c : word;
              d : word;
              x : word;
              s : integer;
              ac : word) return word is
  begin
    return to_stdlogicvector (to_bitvector (a + F(b,c,d) + x + ac) rol s) + b;
  end FF;
  
  function GG(a : word;
              b : word;
              c : word;
              d : word;
              x : word;
              s : integer;
              ac : word) return word is
  begin
    return to_stdlogicvector (to_bitvector (a + G(b,c,d) + x + ac) rol s) + b;
  end GG;
  
  function HH(a : word;
              b : word;
              c : word;
              d : word;
              x : word;
              s : integer;
              ac : word) return word is
  begin
    return to_stdlogicvector (to_bitvector (a + H(b,c,d) + x + ac) rol s) + b;
  end HH;
  
  function II(a : word;
              b : word;
              c : word;
              d : word;
              x : word;
              s : integer;
              ac : word) return word is
  begin
    return to_stdlogicvector (to_bitvector (a + I(b,c,d) + x + ac) rol s) + b;
  end II;

  signal A1 : dataset (0 to 8);
  signal B1 : dataset (0 to 8);
  signal C1 : dataset (0 to 8);
  signal D1 : dataset (0 to 8);

  signal A2 : dataset (0 to 8);
  signal B2 : dataset (0 to 8);
  signal C2 : dataset (0 to 8);
  signal D2 : dataset (0 to 8);

  signal A3 : dataset (0 to 8);
  signal B3 : dataset (0 to 8);
  signal C3 : dataset (0 to 8);
  signal D3 : dataset (0 to 8);

  signal A4 : dataset (0 to 8);
  signal B4 : dataset (0 to 8);
  signal C4 : dataset (0 to 8);
  signal D4 : dataset (0 to 8);


  constant iA : word := x"67452301";
  constant iB : word := x"efcdab89";
  constant iC : word := x"98badcfe";
  constant iD : word := x"10325476";

  signal xx00 : word;
  signal xx01 : word;
  signal xx02 : word;
  signal xx03 : word;

  constant xx04 : word := x"00000080";
  constant xx05 : word := x"00000000";
  constant xx06 : word := x"00000000";
  constant xx07 : word := x"00000000";
  constant xx08 : word := x"00000000";
  constant xx09 : word := x"00000000";
  constant xx10 : word := x"00000000";
  constant xx11 : word := x"00000000";
  constant xx12 : word := x"00000000";
  constant xx13 : word := x"00000000";
  constant xx14 : word := x"000000c0";
  constant xx15 : word := x"00000000";

  constant XXinit : dataset (0 to 15) := (
    x"00000000", x"00000000", x"00000000", x"00000000",
    x"00000000", x"00000000", x"00000000", x"00000000",
    x"00000000", x"00000000", x"00000000", x"00000000",
    x"00000000", x"00000000", x"00000080", x"00000000");
  
  signal Fxx : dataset (0 to 15) := XXinit;
  signal Gxx : dataset (0 to 15) := XXinit;
  signal Hxx : dataset (0 to 15) := XXinit;
  signal Ixx : dataset (0 to 15) := XXinit;

  component delay is
    generic (na : integer; nb : integer);
    port (clk: in std_logic;

          Da: in std_logic_vector (31 downto 0);
          Qa: out std_logic_vector (31 downto 0);

          Db: in std_logic_vector (31 downto 0);
          Qb: out std_logic_vector (31 downto 0));
  end component;

begin
  xx00 <= xxIN;

  Fxx0d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  xx00, Qa=>  Fxx(0),
              Db=>  xx01, Qb=>  Fxx(1), Clk=> Clk);
  Fxx2d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  xx02, Qa=>  Fxx(2),
              Db=>  xx03, Qb=>  Fxx(3), Clk=> Clk);
  Fxx4d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  xx04, Qa=>  Fxx(4),
              Db=>  xx05, Qb=>  Fxx(5), Clk=> Clk);

  Gxx0d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Fxx(0), Qa=>  Gxx(0),
              Db=>  Fxx(1), Qb=>  Gxx(1), Clk=> Clk);
  Gxx2d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Fxx(2), Qa=>  Gxx(2),
              Db=>  Fxx(3), Qb=>  Gxx(3), Clk=> Clk);
  Gxx4d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Fxx(4), Qa=>  Gxx(4),
              Db=>  Fxx(5), Qb=>  Gxx(5), Clk=> Clk);

  Hxx0d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Gxx(0), Qa=>  Hxx(0),
              Db=>  Gxx(1), Qb=>  Hxx(1), Clk=> Clk);
  Hxx2d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Gxx(2), Qa=>  Hxx(2),
              Db=>  Gxx(3), Qb=>  Hxx(3), Clk=> Clk);
  Hxx4d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Gxx(4), Qa=>  Hxx(4),
              Db=>  Gxx(5), Qb=>  Hxx(5), Clk=> Clk);

  Ixx0d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Hxx(0), Qa=>  Ixx(0),
              Db=>  Hxx(1), Qb=>  Ixx(1), Clk=> Clk);
  Ixx2d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Hxx(2), Qa=>  Ixx(2),
              Db=>  Hxx(3), Qb=>  Ixx(3), Clk=> Clk);
  Ixx4d: delay generic map(na=> 5, nb=>5)
    port map (Da=>  Hxx(4), Qa=>  Ixx(4),
              Db=>  Hxx(5), Qb=>  Ixx(5), Clk=> Clk);

  process (Clk)
    variable partA : boolean;
    variable kk1 : dataset (0 to 7);
    variable kk2 : dataset (0 to 7);
    variable kk3 : dataset (0 to 7);
    variable kk4 : dataset (0 to 7);

    variable Fx : dataset (0 to 7);
    variable Gx : dataset (0 to 7);
    variable Hx : dataset (0 to 7);
    variable Ix : dataset (0 to 7);

  begin
    if Clk'event and Clk = '1' then

      xx01 <= xx00;
      xx02 <= xx01;
      xx03 <= xx02;

      -- setup for round 1.
      A1(0) <= phasedCV (iA, A1(8));
      B1(0) <= phasedCV (iB, B1(8));
      C1(0) <= phasedCV (iC, C1(8));
      D1(0) <= phasedCV (iD, D1(8));

      for i in 0 to 3 loop
        Fx (2*i  ) := phasedVC (Fxx((2*i)   mod 16), Fxx((2*i+8) mod 16));
        Fx (2*i+1) := phasedCV (Fxx((2*i+9) mod 16), Fxx((2*i+1) mod 16));
      end loop;

      -- round 1.
      for i in 0 to 7 loop
        kk1(i) := phasedVVa (kk(i), kk(i + 8));

        A1(i+1) <= D1(i);
        B1(i+1) <= FF(A1(i), B1(i), C1(i), D1(i),
                      Fx(i), S1(i mod 4), kk1(i));
        C1(i+1) <= B1(i);
        D1(i+1) <= C1(i);
      end loop;

      -- Setup for round 2.

      A2(0) <= phasedVVb (A1(8), A2 (8));
      B2(0) <= phasedVVb (B1(8), B2 (8));
      C2(0) <= phasedVVb (C1(8), C2 (8));
      D2(0) <= phasedVVb (D1(8), D2 (8));

      for i in 0 to 3 loop
        Gx (2*i  ) := phasedVC (Gxx((2*i)   mod 16), Gxx((2*i+8) mod 16));
        Gx (2*i+1) := phasedCV (Gxx((2*i+9) mod 16), Gxx((2*i+1) mod 16));
      end loop;
      
      -- Round 2
      for i in 0 to 7 loop
        kk2(i) := phasedVVb (kk(i+16), kk(i+24));

        A2(i+1) <= D2(i);
        B2(i+1) <= GG (A2(i), B2(i), C2(i), D2(i),
                       Gx((i*5+1) mod 8), S2(i mod 4), kk2(i));
        C2(i+1) <= B2(i);
        D2(i+1) <= C2(i);
      end loop;

      -- Setup for round 3.

      A3(0) <= phasedVVa (A2(8), A3(8));
      B3(0) <= phasedVVa (B2(8), B3(8));
      C3(0) <= phasedVVa (C2(8), C3(8));
      D3(0) <= phasedVVa (D2(8), D3(8));

      for i in 0 to 3 loop
        Hx (2*i  ) := phasedVC (Hxx((2*i)   mod 16), Hxx((2*i+8) mod 16));
        Hx (2*i+1) := phasedCV (Hxx((2*i+9) mod 16), Hxx((2*i+1) mod 16));
      end loop;

      -- Round 3
      for i in 0 to 7 loop
        kk3(i) := phasedVVa (kk(i+32), kk(i+40));

        A3(i+1) <= D3(i);
        B3(i+1) <= HH (A3(i), B3(i), C3(i), D3(i),
                       Hx((i*3+5) mod 8), S3(i mod 4), kk3(i));
        C3(i+1) <= B3(i);
        D3(i+1) <= C3(i);
      end loop;

      -- Setup for round 4.

      A4(0) <= phasedVVb (A3(8), A4(8));
      B4(0) <= phasedVVb (B3(8), B4(8));
      C4(0) <= phasedVVb (C3(8), C4(8));
      D4(0) <= phasedVVb (D3(8), D4(8));

      for i in 0 to 7 loop
        A4(i+1) <= D4(i);
        C4(i+1) <= B4(i);
        D4(i+1) <= C4(i);
      end loop;

      for i in 0 to 3 loop
        Ix (2*i  ) := phasedVC (Ixx((2*i  ) mod 16), Ixx((2*i+8) mod 16));
        Ix (2*i+1) := phasedCV (Ixx((2*i+9) mod 16), Ixx((2*i+1) mod 16));
      end loop;
      
      -- Round 4
      for i in 0 to 7 loop
        kk4(i) := phasedVVb (kk(i+48), kk(i+56));

        A4(i+1) <= D4(i);
        B4(i+1) <= II (A4(i), B4(i), C4(i), D4(i),
                       Ix ((i*7) mod 8), S4(i mod 4), kk4(i));
        C4(i+1) <= B4(i);
        D4(i+1) <= C4(i);
      end loop;
      
      Aout <= A4(8) + iA;
      Bout <= B4(8) + iB;
      Cout <= C4(8) + iC;
      Dout <= D4(8) + iD;
    end if;
  end process;
end Behavioral;
