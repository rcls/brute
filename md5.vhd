
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

entity delay is
  generic (na : integer; nb : integer);

  port (clk: in std_logic;
        Da: in std_logic_vector (31 downto 0);
        Qa: out std_logic_vector (31 downto 0);
        Db: in std_logic_vector (31 downto 0);
        Qb: out std_logic_vector (31 downto 0));
end delay;

architecture Behavioral of delay is
  signal counta : std_logic_vector (4 downto 0) := "00000";
  signal countb : std_logic_vector (4 downto 0) := "00000";
  
  constant lima : std_logic_vector (4 downto 0) := conv_std_logic_vector (na - 2, 5);
  constant limb : std_logic_vector (4 downto 0) := conv_std_logic_vector (nb - 2, 5);

  subtype word is std_logic_vector (31 downto 0);
  type mem_t is array (31 downto 0) of word;
  signal mema : mem_t;
  signal memb : mem_t;
begin
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      Qa <= mema (conv_integer (counta));
      mema (conv_integer (counta)) <= Da;
      Qb <= memb (conv_integer (countb));
      memb (conv_integer (countb)) <= Db;
      
      if counta = lima then
        counta <= "00000";
      else
        counta <= counta + 1;
      end if;

      if countb = limb then
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
  Port (x0 : in  std_logic_vector (31 downto 0);
        x1 : in  std_logic_vector (31 downto 0);
        x2 : in  std_logic_vector (31 downto 0);
        x3 : in  std_logic_vector (31 downto 0);
        x4 : in  std_logic_vector (31 downto 0);
        x5 : in  std_logic_vector (31 downto 0);
        A64 : out std_logic_vector (31 downto 0);
        Aout : out std_logic_vector (31 downto 0);
        Bout : out std_logic_vector (31 downto 0);
        Cout : out std_logic_vector (31 downto 0);
        Dout : out std_logic_vector (31 downto 0);
        Clk : in std_logic);
end md5;


architecture Behavioral of md5 is
  subtype word is std_logic_vector (31 downto 0);
  
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
    x"00000080", x"00000000", x"00000000", x"00000000",
    x"00000000", x"00000000", x"00000000", x"00000000",
    x"00000000", x"00000000", x"00000080", x"00000000");
  
  signal Fx : dataset (0 to 15) := Xinit;
  signal Gx : dataset (0 to 15) := Xinit;
  signal Hx : dataset (0 to 15) := Xinit;
  signal Ix : dataset (0 to 15) := Xinit;

  constant iA : word := x"67452301";
  constant iB : word := x"efcdab89";
  constant iC : word := x"98badcfe";
  constant iD : word := x"10325476";

  component delay is
    generic (na : integer; nb : integer);
    port (clk: in std_logic;

          Da: in std_logic_vector (31 downto 0);
          Qa: out std_logic_vector (31 downto 0);

          Db: in std_logic_vector (31 downto 0);
          Qb: out std_logic_vector (31 downto 0));
  end component;

begin
  Fx(0) <= x0;
  
  A(0) <= iA;
  B(0) <= iB;
  C(0) <= iC;
  D(0) <= iD;

  -- Early access to output for comparator.
  A64 <= A(64);
      
  -- The actual outputs; let our user do the registering.
  Aout <= A(64) + iA;
  Bout <= B(64) + iB;
  Cout <= C(64) + iC;
  Dout <= D(64) + iD;

--  Fx0d: delay generic map(na=> 1, nb=>1)
--    port map (Da=>  x0,   Qa=>  open,  Db=>  x1, Qb=>  Fx(1), Clk=> Clk);
  Fx2d: delay generic map(na=> 2, nb=>3)
    port map (Da=>  x2,   Qa=>  Fx(2), Db=>  x3, Qb=>  Fx(3), Clk=> Clk);
  Fx4d: delay generic map(na=> 4, nb=>5)
    port map (Da=>  x4,   Qa=>  Fx(4), Db=>  x5, Qb=>  Fx(5), Clk=> Clk);

  Gx0d: delay generic map(na=> D12(0), nb=>D12(1))
    port map (Da=>  Fx(0), Qa=>  Gx(0), Db=>  Fx(1), Qb=>  Gx(1), Clk=> Clk);
  Gx2d: delay generic map(na=> D12(2), nb=>D12(3))
    port map (Da=>  Fx(2), Qa=>  Gx(2), Db=>  Fx(3), Qb=>  Gx(3), Clk=> Clk);
  Gx4d: delay generic map(na=> D12(4), nb=>D12(5))
    port map (Da=>  Fx(4), Qa=>  Gx(4), Db=>  Fx(5), Qb=>  Gx(5), Clk=> Clk);

  Hx0d: delay generic map(na=> D23(0), nb=>D23(1))
    port map (Da=>  Gx(0), Qa=>  Hx(0), Db=>  Gx(1), Qb=>  Hx(1), Clk=> Clk);
  Hx2d: delay generic map(na=> D23(2), nb=>D23(3))
    port map (Da=>  Gx(2), Qa=>  Hx(2), Db=>  Gx(3), Qb=>  Hx(3), Clk=> Clk);
  Hx4d: delay generic map(na=> D23(4), nb=>D23(5))
    port map (Da=>  Gx(4), Qa=>  Hx(4), Db=>  Gx(5), Qb=>  Hx(5), Clk=> Clk);

  Ix0d: delay generic map(na=> D34(0), nb=>D34(1))
    port map (Da=>  Hx(0), Qa=>  Ix(0), Db=>  Hx(1), Qb=>  Ix(1), Clk=> Clk);
  Ix2d: delay generic map(na=> D34(2), nb=>D34(3))
    port map (Da=>  Hx(2), Qa=>  Ix(2), Db=>  Hx(3), Qb=>  Ix(3), Clk=> Clk);
  Ix4d: delay generic map(na=> D34(4), nb=>D34(5))
    port map (Da=>  Hx(4), Qa=>  Ix(4), Db=>  Hx(5), Qb=>  Ix(5), Clk=> Clk);

  process (Clk)
  begin
    if Clk'event and Clk = '1' then

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
