
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;


entity md5 is
  Port (in0 : in  std_logic_vector (31 downto 0);
        in1 : in  std_logic_vector (31 downto 0);
        in2 : in  std_logic_vector (31 downto 0);
        hit : out std_logic;            -- Do we have n zeros for some n.
        Aout : out std_logic_vector (31 downto 0);
        Bout : out std_logic_vector (31 downto 0);
        Cout : out std_logic_vector (31 downto 0);
        Dout : out std_logic_vector (31 downto 0);
        Clk : in std_logic);
end md5;


architecture Behavioral of md5 is
  subtype word is std_logic_vector (31 downto 0);
  subtype nibble_t is std_logic_vector (3 downto 0);
  subtype byte_t is std_logic_vector (7 downto 0);
  
  type dataset is array (natural range <>) of word;

  type iarray is array (natural range <>) of integer;

  constant S1 : iarray (0 to 3) := (7, 12, 17, 22);
  constant S2 : iarray (0 to 3) := (5, 9, 14, 20);
  constant S3 : iarray (0 to 3) := (4, 11, 16, 23);
  constant S4 : iarray (0 to 3) := (6, 10, 15, 21);

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

  function hexify (n : nibble_t) return byte_t is
    variable alpha : std_logic := n(3) and (n(2) or n(1));
    variable result : byte_t;
  begin
    result (7) := '0';
    result (6) := alpha;
    result (5) := not alpha;
    result (4) := not alpha;
    result (3) := n(3) and not n(2) and not n(1);
    result (2 downto 0) := n(2 downto 0) - alpha;
    return result;
  end hexify;

  function hexify16 (n : std_logic_vector (15 downto 0)) return word is
    variable result : word;
  begin
    result( 7 downto  0) := hexify (n ( 3 downto  0));
    result(15 downto  8) := hexify (n ( 7 downto  4));
    result(23 downto 16) := hexify (n (11 downto  8));
    result(31 downto 24) := hexify (n (15 downto 12));
    return result;
  end hexify16;

  function F(x : word; y : word; z : word)
    return word is
    variable r : word := (x and y) or (z and not x);
  begin
    return r;
  end F;

  function G(x : word; y : word; z : word)
    return word is
    variable r : word := (z and x) or (y and not z);
  begin
    return r;
  end G;

  function H(x : word; y : word; z : word)
    return word is
    variable r : word := x xor y xor z;
  begin
    return r;
  end H;

  function I(x : word; y : word; z : word)
    return word is
    variable r : word := y xor (x or not z);
  begin
    return r;
  end I;

  function FF(a : word;
              b : word;
              c : word;
              d : word;
              x : word;
              s : integer;
              ac : word) return word is
--    constant r : word := to_stdlogicvector (to_bitvector (a + F(b,c,d) + x + ac) rol s) + b;
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

  -- Return the stage at which input #i is used in round 1.
  function U1 (i : integer) return integer is
  begin
    return i;
  end;

  -- Return the stage at which input #i is used in round 2.
  function U2 (i : integer) return integer is
  begin
    return 16 + (13 * i + 3) mod 16;
  end;

  -- Return the stage at which input #i is used in round 3.
  function U3 (i : integer) return integer is
  begin
    return 32 + (11 * i + 9) mod 16;
  end;

  -- Return the stage at which input #i is used in round 4.
  function U4 (i : integer) return integer is
  begin
    return 48 + (7 * i) mod 16;
  end;

  -- Round 1 to round 2 delay.
  function D12 (i : integer) return integer is
  begin
    return U2(i) - U1(i);
  end;

  -- Round 2 to round 3 delay.
  function D23 (i : integer) return integer is
  begin
    return U3(i) - U2(i);
  end;

  -- Round 3 to round 4 delay.
  function D34 (i : integer) return integer is
  begin
    return U4(i) - U3(i);
  end;

  signal A : dataset (0 to 64);
  signal B : dataset (0 to 64);
  signal C : dataset (0 to 64);
  signal D : dataset (0 to 64);

  constant Xinit : dataset (0 to 15) := (
    x"00000000", x"00000000", x"00000000", x"00000000",
    x"00000000", x"00000000", x"00000000", x"00000000",
    x"00000000", x"00000000", x"00000000", x"00000000",
    x"00000000", x"00000000", x"00000088", x"00000000");

  signal Fx : dataset (0 to 15) := Xinit;
  signal Gx : dataset (0 to 15) := Xinit;
  signal Hx : dataset (0 to 15) := Xinit;
  signal Ix : dataset (0 to 15) := Xinit;

  -- Formatted input.
  signal x0 : word;
  signal x1 : word;
  signal x2 : word;
  signal x3 : word;
  signal x4 : word;
  signal x5 : word;

  constant iA : word := x"67452301";
  constant iB : word := x"efcdab89";
  constant iC : word := x"98badcfe";
  constant iD : word := x"10325476";

  constant iAneg : word := x"00000000" - iA;

  component delay is
    generic (N : integer);
    port (clk: in std_logic;

          D: in std_logic_vector (31 downto 0);
          Q: out std_logic_vector (31 downto 0));
  end component;

begin
  Fx(0) <= x0;

  A(0) <= iA;
  B(0) <= iB;
  C(0) <= iC;
  D(0) <= iD;

  -- We flag outputs with 24 zeros.
  hit <= '1' when A(64)(23 downto 0) = iAneg(23 downto 0) else '0';

  -- The actual outputs; let our user do the registering.
  Aout <= A(64) + iA;
  Bout <= B(64) + iB;
  Cout <= C(64) + iC;
  Dout <= D(64) + iD;

--  Fx0d: delay generic map(na=> 1, nb=>1)
--    port map (D=>  x0,   Qa=>  open,  Db=>  x1, Qb=>  Fx(1), Clk=> Clk);
  Fx2d: delay generic map(N=>2)      port map (D=> x2,    Q=> Fx(2), Clk=> Clk);
  Fx3d: delay generic map(N=>3)      port map (D=> x3,    Q=> Fx(3), Clk=> Clk);
  Fx4d: delay generic map(N=>4)      port map (D=> x4,    Q=> Fx(4), Clk=> Clk);
  Fx5d: delay generic map(N=>5)      port map (D=> x5,    Q=> Fx(5), Clk=> Clk);

  Gx0d: delay generic map(N=>D12(0)) port map (D=> Fx(0), Q=> Gx(0), Clk=> Clk);
  Gx1d: delay generic map(N=>D12(1)) port map (D=> Fx(1), Q=> Gx(1), Clk=> Clk);
  Gx2d: delay generic map(N=>D12(2)) port map (D=> Fx(2), Q=> Gx(2), Clk=> Clk);
  Gx3d: delay generic map(N=>D12(3)) port map (D=> Fx(3), Q=> Gx(3), Clk=> Clk);
  Gx4d: delay generic map(N=>D12(4)) port map (D=> Fx(4), Q=> Gx(4), Clk=> Clk);
  Gx5d: delay generic map(N=>D12(5)) port map (D=> Fx(5), Q=> Gx(5), Clk=> Clk);

  Hx0d: delay generic map(N=>D23(0)) port map (D=> Gx(0), Q=> Hx(0), Clk=> Clk);
  Hx1d: delay generic map(N=>D23(1)) port map (D=> Gx(1), Q=> Hx(1), Clk=> Clk);
  Hx2d: delay generic map(N=>D23(2)) port map (D=> Gx(2), Q=> Hx(2), Clk=> Clk);
  Hx3d: delay generic map(N=>D23(3)) port map (D=> Gx(3), Q=> Hx(3), Clk=> Clk);
  Hx4d: delay generic map(N=>D23(4)) port map (D=> Gx(4), Q=> Hx(4), Clk=> Clk);
  Hx5d: delay generic map(N=>D23(5)) port map (D=> Gx(5), Q=> Hx(5), Clk=> Clk);

  Ix0d: delay generic map(N=>D34(0)) port map (D=> Hx(0), Q=> Ix(0), Clk=> Clk);
  Ix1d: delay generic map(N=>D34(1)) port map (D=> Hx(1), Q=> Ix(1), Clk=> Clk);
  Ix2d: delay generic map(N=>D34(2)) port map (D=> Hx(2), Q=> Ix(2), Clk=> Clk);
  Ix3d: delay generic map(N=>D34(3)) port map (D=> Hx(3), Q=> Ix(3), Clk=> Clk);
  Ix4d: delay generic map(N=>D34(4)) port map (D=> Hx(4), Q=> Ix(4), Clk=> Clk);
  Ix5d: delay generic map(N=>D34(5)) port map (D=> Hx(5), Q=> Ix(5), Clk=> Clk);

  process (Clk)
  begin
    if Clk'event and Clk = '1' then

      -- Load x0 through x5 with the hexified inputs.
      x0 <= hexify16 (in0 (15 downto  0));
      x1 <= hexify16 (in0 (31 downto 16));
      x2 <= hexify16 (in1 (15 downto  0));
      x3 <= hexify16 (in1 (31 downto 16));
      x4(7 downto 0) <= hexify (in2 (3 downto  0));
      x4(15 downto 8) <= x"80";
      x4(31 downto 16) <= x"0000";
      x5 <= x"00000000";
      
      Fx(1) <= x1; -- gives 1 cycle delay.

      -- I don't see why these are necessary but the simulator seems to need
      -- them.
      A(0) <= iA;
      B(0) <= iB;
      C(0) <= iC;
      D(0) <= iD;

      -- Propagations.
      for i in 0 to 63 loop
        A(i+1) <= D(i);
        C(i+1) <= B(i);
        D(i+1) <= C(i);
      end loop;

      -- round 1.
      for i in 0 to 15 loop
        B(i+1) <= FF (A(i), B(i), C(i), D(i), Fx(i), S1(i mod 4), kk(i));
      end loop;

      -- Round 2
      for i in 16 to 31 loop
        B(i+1) <= GG (A(i), B(i), C(i), D(i), Gx ((i*5+1) mod 16), S2(i mod 4), kk(i));
      end loop;

      -- Round 3
      for i in 32 to 47 loop
        B(i+1) <= HH (A(i), B(i), C(i), D(i), Hx ((i*3+5) mod 16), S3(i mod 4), kk(i));
      end loop;

      -- Round 4
      for i in 48 to 63 loop
        B(i+1) <= II (A(i), B(i), C(i), D(i),
                      Ix ((i*7) mod 16), S4(i mod 4), kk(i));
      end loop;
    end if;
  end process;
end Behavioral;
