
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
library UNISIM;
use UNISIM.VComponents.all;

library work;
use work.defs.all;

entity md5 is
  port (input : in word96_t;
        output : out word128_t;
        bmon : out dataset_t (0 to 64);
        Clk : in std_logic);
end md5;


architecture Behavioral of md5 is
  type iarray is array (natural range <>) of integer;

  constant S : iarray (0 to 63) := (
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21);

  constant KK : dataset_t (0 to 63) := (
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

  function FF(x : word_t; y : word_t; z : word_t)
    return word_t is
    variable r : word_t := (x and y) or (z and not x);
  begin
    return r;
  end FF;

  function GG(x : word_t; y : word_t; z : word_t)
    return word_t is
    variable r : word_t := (z and x) or (y and not z);
  begin
    return r;
  end GG;

  function HH(x : word_t; y : word_t; z : word_t)
    return word_t is
    variable r : word_t := x xor y xor z;
  begin
    return r;
  end HH;

  function II(x : word_t; y : word_t; z : word_t)
    return word_t is
    variable r : word_t := y xor (x or not z);
  begin
    return r;
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
    return (U2(i) - U1(i)) * 3;
  end;

  -- Round 2 to round 3 delay.
  function D23 (i : integer) return integer is
  begin
    return (U3(i) - U2(i)) * 3;
  end;

  -- Round 3 to round 4 delay.
  function D34 (i : integer) return integer is
  begin
    return (U4(i) - U3(i)) * 3;
  end;

  -- Map a round number to the yy index it uses.
  function index (n : integer) return integer is
  begin
    if n < 16 then
      return n;
    elsif n < 32 then
      return ((n * 5 + 1) mod 16) + 16;
    elsif n < 48 then
      return ((n * 3 + 5) mod 16) + 32;
    else
      return ((n * 7) mod 16) + 48;
    end if;
  end;

  -- Should a round use a DSP?  Take up 21 DSP pairs per pipeline.
  function use_dsp (n : integer) return boolean is
    variable idx : integer := index (n);
  begin
    return idx mod 16 >= 8 and idx mod 16 <= 12;
  end;

  signal A : dataset_t (0 to 64);
  signal B : dataset_t (0 to 64);
  signal C : dataset_t (0 to 64);
  signal D : dataset_t (0 to 64);

  signal Ba : dataset_t (0 to 63);
  signal Bb : dataset_t (0 to 63);
  signal Ca : dataset_t (0 to 63);
  signal Cb : dataset_t (0 to 63);
  signal Da : dataset_t (0 to 63);
  signal Db : dataset_t (0 to 63);

  signal func : dataset_t (0 to 63);
  signal sum  : dataset_t (0 to 63);
  signal sum1 : dataset_t (0 to 63);
  signal sum2 : dataset_t (0 to 63);

  signal Fx : dataset_t (0 to 5);
  signal Gx : dataset_t (0 to 5);
  signal Hx : dataset_t (0 to 5);
  signal Ix : dataset_t (0 to 5);

  -- Formatted input.
  signal xx : dataset_t (0 to 5);

  -- Delayed inputs, unpermuted.
  signal yy : dataset_t (0 to 63);

  constant iA : word_t := x"67452301";
  constant iB : word_t := x"efcdab89";
  constant iC : word_t := x"98badcfe";
  constant iD : word_t := x"10325476";

  constant iAneg : word_t := x"00000000" - iA;

  component delay is
    generic (N : integer; extra_buf : integer := 0);
    port (clk: in std_logic; D: in word_t; Q: out word_t);
  end component;

  -- Compute (OneA + OneB) + Two using DSPs.  We do this for rounds 6 to 10
  -- of each stage.
  component adder3 is
    port (addend2 : in word_t; addend4 : in word_t; addend5 : in word_t;
          Sum : out word_t; Clk : std_logic);
  end component;

begin

  A(0) <= iA;
  B(0) <= iB;
  C(0) <= iC;
  D(0) <= iD;

  bmon <= B;

  -- The actual outputs; we register these as adder->logic->ram is a bottleneck.
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      output ( 31 downto  0) <= A(64) + iA;
      output ( 63 downto 32) <= B(64) + iB;
      output ( 95 downto 64) <= C(64) + iC;
      output (127 downto 96) <= D(64) + iD;
    end if;
  end process;

  --  Fx0d: delay generic map(na=> 1, nb=>1)
  --    port map (D=>  x0,   Qa=>  open,  Db=>  x1, Qb=>  Fx(1), Clk=> Clk);
  FxdGen: for i in 1 to 5 generate
    Fxd: delay generic map(N=> 3*i+1) port map (D=> xx(i), Q=> Fx(i), Clk=>Clk);
  end generate;

  Gxd: delay generic map(N=>D12(0), extra_buf => 1)
    port map (D=> Fx(0), Q=> Gx(0), Clk=>Clk);
  Hxd: delay generic map(N=>D23(0), extra_buf => 1)
    port map (D=> Gx(0), Q=> Hx(0), Clk=>Clk);
  Ixd: delay generic map(N=>D34(0), extra_buf => 1)
    port map (D=> Hx(0), Q=> Ix(0), Clk=>Clk);

  XxdGen: for i in 1 to 5 generate
    Gxd: delay generic map(N=>D12(i)) port map (D=> Fx(i), Q=> Gx(i), Clk=>Clk);
    Hxd: delay generic map(N=>D23(i)) port map (D=> Gx(i), Q=> Hx(i), Clk=>Clk);
    Ixd: delay generic map(N=>D34(i)) port map (D=> Hx(i), Q=> Ix(i), Clk=>Clk);
  end generate;

  -- Load x0 through x5 with the hexified inputs.
  genhex : for i in 0 to 5 generate
    xx(i) <= hexword (input, i);
  end generate;

  input_mask: for i in 0 to 5 generate
    yy(i)      <= (maskw0(i) or Fx(i)) and maskw1(i);
    yy(i + 16) <= (maskw0(i) or Gx(i)) and maskw1(i);
    yy(i + 32) <= (maskw0(i) or Hx(i)) and maskw1(i);
    yy(i + 48) <= (maskw0(i) or Ix(i)) and maskw1(i);
  end generate;

  input_constants: for i in 0 to 3 generate
	 yy(i * 16 + 6) <= maskw0(6); -- x80 for >=92 bits.
    input_zeros: for j in 7 to 13 generate
      yy(i * 16 + j) <= x"00000000";
    end generate;
    yy(i * 16 + 14) <= conv_std_logic_vector ((bits + 3) / 4, 29) & "000";
    yy(i * 16 + 15) <= x"00000000";
  end generate;

  -- The functions for MD5 rounds.
  round_func: for i in 0 to 15 generate
    func(i)    <= FF (B(i),    C(i),    D(i));
    func(i+16) <= GG (B(i+16), C(i+16), D(i+16));
    func(i+32) <= HH (B(i+32), C(i+32), D(i+32));
    func(i+48) <= II (B(i+48), C(i+48), D(i+48));
  end generate;

  -- The round sums.  For each round we generate sum(i) as
  -- func(i)+yy(index(i))+kk(i)+a(i) with a two cycle latency.  The strategy
  -- depends on index(i).
  round_sum: for i in 0 to 63 generate
    data_round: if index (i) mod 16 < 6 generate
      process (Clk) -- Only kk is constant.
      begin
        if Clk'event and Clk = '1' then
          sum1(i) <= func(i) + kk(i);
          sum2(i) <= yy(index(i)) + A(i);
          sum(i) <= sum1(i) + sum2(i);
        end if;
      end process;
    end generate;
    dsp_round: if use_dsp (i) generate
      add : adder3
        port map (
          addend2=> func(i),
          addend4=> kk(i), -- yy(i) is zero, kk(i) is constant.
          addend5=> D(i-1), -- A(i) one round prior.
          Sum => sum(i),
          Clk => Clk);
    end generate;
    by_hand_round: if index (i) mod 16 >= 6 and not use_dsp(i) generate
      process (Clk)
      begin
        if Clk'event and Clk = '1' then
          sum1(i) <= A(i); -- another cycle of delay.
          sum2(i) <= func(i) + (yy(index(i)) + kk(i));
          sum(i) <= sum1(i) + sum2(i);
        end if;
      end process;
    end generate;
  end generate;

  process (Clk)
    variable func : word_t;
  begin
    if Clk'event and Clk = '1' then

      Fx(0) <= xx(0); -- single cycle delay.

      A(0) <= iA;
      B(0) <= iB;
      C(0) <= iC;
      D(0) <= iD;

      -- Propagations.
      Ba <= B(0 to 63);
      Ca <= C(0 to 63);
      Da <= D(0 to 63);
      Bb <= Ba;
      Cb <= Ca;
      Db <= Da;
      for i in 0 to 63 loop
        A(i+1) <= Db(i);
        C(i+1) <= Bb(i);
        D(i+1) <= Cb(i);
        -- round output.
        B(i+1) <= rotl (sum(i), S(i)) + Bb(i);
      end loop;
    end if;
  end process;
end Behavioral;
