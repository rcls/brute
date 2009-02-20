----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    07:01:24 02/21/2009 
-- Design Name: 
-- Module Name:    md5 - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity md5 is
  Port (xxIN : in  STD_LOGIC_VECTOR (31 downto 0);
        Aout : out  STD_LOGIC_VECTOR (31 downto 0);
        Bout : out  STD_LOGIC_VECTOR (31 downto 0);
        Cout : out  STD_LOGIC_VECTOR (31 downto 0);
        Dout : out  STD_LOGIC_VECTOR (31 downto 0);
        Clk : in std_logic);
end md5;

--#define F(x, y, z) OR (AND ((x), (y)), ANDN ((x), (z)))
--#define G(x, y, z) OR (AND ((z), (x)), ANDN ((z), (y)))
--#define H(x, y, z) XOR ((x), XOR ((y), (z)))
--#define I(x, y, z) XOR ((y), OR ((x), NOT ((z))))

architecture Behavioral of md5 is

  constant S11 : integer := 7;
  constant S12 : integer := 12;
  constant S13 : integer := 17;
  constant S14 : integer := 22;
  constant S21 : integer := 5;
  constant S22 : integer := 9;
  constant S23 : integer := 14;
  constant S24 : integer := 20;
  constant S31 : integer := 4;
  constant S32 : integer := 11;
  constant S33 : integer := 16;
  constant S34 : integer := 23;
  constant S41 : integer := 6;
  constant S42 : integer := 10;
  constant S43 : integer := 15;
  constant S44 : integer := 21;

--alias u32 is std_logic_vector;

  function F(x : std_logic_vector; y : std_logic_vector; z : std_logic_vector)
    return std_logic_vector is
  begin
    return (x and y) or (x and not z);
  end F;

  function G(x : std_logic_vector; y : std_logic_vector; z : std_logic_vector)
    return std_logic_vector is
  begin
    return (z and x) or (z and not y);
  end G;

  function H(x : std_logic_vector; y : std_logic_vector; z : std_logic_vector)
    return std_logic_vector is
  begin
    return x xor y xor z;
  end H;

  function I(x : std_logic_vector; y : std_logic_vector; z : std_logic_vector)
    return std_logic_vector is
  begin
    return y xor (x or not z);
  end I;


--#define GENERATE(FUN,a, b, c, d, x, s, ac) {		\
--	(a) = A4 ((a), FUN ((b), (c), (d)), (x), DIAG (ac));	\
--	(a) = ROTATE_LEFT ((a), (s));			\
--        (a) = ADD ((a), (b));				\
--    }
  function FF(a : std_logic_vector;
              b : std_logic_vector;
              c : std_logic_vector;
              d : std_logic_vector;
              x : std_logic_vector;
              s : integer;
              ac : std_logic_vector) return std_logic_vector is
  begin
    return to_stdlogicvector (to_bitvector (a + F(b,c,d) + x + ac) rol s) + b;
  end FF;
  
  function GG(a : std_logic_vector;
              b : std_logic_vector;
              c : std_logic_vector;
              d : std_logic_vector;
              x : std_logic_vector;
              s : integer;
              ac : std_logic_vector) return std_logic_vector is
  begin
    return to_stdlogicvector (to_bitvector (a + G(b,c,d) + x + ac) rol s) + b;
  end GG;
  
  function HH(a : std_logic_vector;
              b : std_logic_vector;
              c : std_logic_vector;
              d : std_logic_vector;
              x : std_logic_vector;
              s : integer;
              ac : std_logic_vector) return std_logic_vector is
  begin
    return to_stdlogicvector (to_bitvector (a + H(b,c,d) + x + ac) rol s) + b;
  end HH;
  
  function II(a : std_logic_vector;
              b : std_logic_vector;
              c : std_logic_vector;
              d : std_logic_vector;
              x : std_logic_vector;
              s : integer;
              ac : std_logic_vector) return std_logic_vector is
  begin
    return to_stdlogicvector (to_bitvector (a + I(b,c,d) + x + ac) rol s) + b;
  end II;

  type datapipe is array (64 downto 0) of std_logic_vector (31 downto 0);
  signal A : datapipe;
  signal B : datapipe;
  signal C : datapipe;
  signal D : datapipe;

  signal xx00 : STD_LOGIC_VECTOR (31 downto 0);
  signal xx01 : STD_LOGIC_VECTOR (31 downto 0);
  signal xx02 : STD_LOGIC_VECTOR (31 downto 0);
  signal xx03 : STD_LOGIC_VECTOR (31 downto 0);

  constant iA : std_logic_vector (31 downto 0) := x"67452301";
  constant iB : std_logic_vector (31 downto 0) := x"efcdab89";
  constant iC : std_logic_vector (31 downto 0) := x"98badcfe";
  constant iD : std_logic_vector (31 downto 0) := x"10325476";

  constant xx04 : std_logic_vector (31 downto 0) := x"00000080";
  constant xx05 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx06 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx07 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx08 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx09 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx10 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx11 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx12 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx13 : std_logic_vector (31 downto 0) := x"00000000";
  constant xx14 : std_logic_vector (31 downto 0) := x"000000c0";
  constant xx15 : std_logic_vector (31 downto 0) := x"00000000";
  
begin
  A(0) <= iA;
  B(0) <= iB;
  C(0) <= iC;
  D(0) <= iD;

  xx00 <= xxIN;
  
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      -- Default is to carry forwards.
      for i in 0 to 63 loop
        A(i+1) <= A(i);
        B(i+1) <= B(i);
        C(i+1) <= C(i);
        D(i+1) <= D(i);
      end loop;

      xx01 <= xx00;
      xx02 <= xx01;
      xx03 <= xx02;
      
      -- Round 1
      A(01) <= FF (A(00), B(00), C(00), D(00), xx00, S11, x"d76aa478"); -- 1
      D(02) <= FF (D(01), A(01), B(01), C(01), xx01, S12, x"e8c7b756"); -- 2
      C(03) <= FF (C(02), D(02), A(02), B(02), xx02, S13, x"242070db"); -- 3
      B(04) <= FF (B(03), C(03), D(03), A(03), xx03, S14, x"c1bdceee"); -- 4
      A(05) <= FF (A(04), B(04), C(04), D(04), xx04, S11, x"f57c0faf"); -- 5
      D(06) <= FF (D(05), A(05), B(05), C(05), xx05, S12, x"4787c62a"); -- 6
      C(07) <= FF (C(06), D(06), A(06), B(06), xx06, S13, x"a8304613"); -- 7
      B(08) <= FF (B(07), C(07), D(07), A(07), xx07, S14, x"fd469501"); -- 8
      A(09) <= FF (A(08), B(08), C(08), D(08), xx08, S11, x"698098d8"); -- 9
      D(10) <= FF (D(09), A(09), B(09), C(09), xx09, S12, x"8b44f7af"); -- 10
      C(11) <= FF (C(10), D(10), A(10), B(10), xx10, S13, x"ffff5bb1"); -- 11
      B(12) <= FF (B(11), C(11), D(11), A(11), xx11, S14, x"895cd7be"); -- 12
      A(13) <= FF (A(12), B(12), C(12), D(12), xx12, S11, x"6b901122"); -- 13
      D(14) <= FF (D(13), A(13), B(13), C(13), xx13, S12, x"fd987193"); -- 14
      C(15) <= FF (C(14), D(14), A(14), B(14), xx14, S13, x"a679438e"); -- 15
      B(16) <= FF (B(15), C(15), D(15), A(15), xx15, S14, x"49b40821"); -- 16

      -- Round 2
      A(17) <= GG (A(16), B(16), C(16), D(16), xx01, S21, x"f61e2562"); -- 17
      D(18) <= GG (D(17), A(17), B(17), C(17), xx06, S22, x"c040b340"); -- 18
      C(19) <= GG (C(18), D(18), A(18), B(18), xx11, S23, x"265e5a51"); -- 19
      B(20) <= GG (B(19), C(19), D(19), A(19), xx00, S24, x"e9b6c7aa"); -- 20
      A(21) <= GG (A(20), B(20), C(20), D(20), xx05, S21, x"d62f105d"); -- 21
      D(22) <= GG (D(21), A(21), B(21), C(21), xx10, S22, x"02441453"); -- 22
      C(23) <= GG (C(22), D(22), A(22), B(22), xx15, S23, x"d8a1e681"); -- 23
      B(24) <= GG (B(23), C(23), D(23), A(23), xx04, S24, x"e7d3fbc8"); -- 24
      A(25) <= GG (A(24), B(24), C(24), D(24), xx09, S21, x"21e1cde6"); -- 25
      D(26) <= GG (D(25), A(25), B(25), C(25), xx14, S22, x"c33707d6"); -- 26
      C(27) <= GG (C(26), D(26), A(26), B(26), xx03, S23, x"f4d50d87"); -- 27
      B(28) <= GG (B(27), C(27), D(27), A(27), xx08, S24, x"455a14ed"); -- 28
      A(29) <= GG (A(28), B(28), C(28), D(28), xx13, S21, x"a9e3e905"); -- 29
      D(30) <= GG (D(29), A(29), B(29), C(29), xx02, S22, x"fcefa3f8"); -- 30
      C(31) <= GG (C(30), D(30), A(30), B(30), xx07, S23, x"676f02d9"); -- 31
      B(32) <= GG (B(31), C(31), D(31), A(31), xx12, S24, x"8d2a4c8a"); -- 32

      -- Round 3
      A(33) <= HH (A(32), B(32), C(32), D(32), xx05, S31, x"fffa3942"); -- 33
      D(34) <= HH (D(33), A(33), B(33), C(33), xx08, S32, x"8771f681"); -- 34
      C(35) <= HH (C(34), D(34), A(34), B(34), xx11, S33, x"6d9d6122"); -- 35
      B(36) <= HH (B(35), C(35), D(35), A(35), xx14, S34, x"fde5380c"); -- 36
      A(37) <= HH (A(36), B(36), C(36), D(36), xx01, S31, x"a4beea44"); -- 37
      D(38) <= HH (D(37), A(37), B(37), C(37), xx04, S32, x"4bdecfa9"); -- 38
      C(39) <= HH (C(38), D(38), A(38), B(38), xx07, S33, x"f6bb4b60"); -- 39
      B(40) <= HH (B(39), C(39), D(39), A(39), xx10, S34, x"bebfbc70"); -- 40
      A(41) <= HH (A(40), B(40), C(40), D(40), xx13, S31, x"289b7ec6"); -- 41
      D(42) <= HH (D(41), A(41), B(41), C(41), xx00, S32, x"eaa127fa"); -- 42
      C(43) <= HH (C(42), D(42), A(42), B(42), xx03, S33, x"d4ef3085"); -- 43
      B(44) <= HH (B(43), C(43), D(43), A(43), xx06, S34, x"04881d05"); -- 44
      A(45) <= HH (A(44), B(44), C(44), D(44), xx09, S31, x"d9d4d039"); -- 45
      D(46) <= HH (D(45), A(45), B(45), C(45), xx12, S32, x"e6db99e5"); -- 46
      C(47) <= HH (C(46), D(46), A(46), B(46), xx15, S33, x"1fa27cf8"); -- 47
      B(48) <= HH (B(47), C(47), D(47), A(47), xx02, S34, x"c4ac5665"); -- 48

      -- Round 4
      A(49) <= II (A(48), B(48), C(48), D(48), xx00, S41, x"f4292244"); -- 49
      D(50) <= II (D(49), A(49), B(49), C(49), xx07, S42, x"432aff97"); -- 50
      C(51) <= II (C(50), D(50), A(50), B(50), xx14, S43, x"ab9423a7"); -- 51
      B(52) <= II (B(51), C(51), D(51), A(51), xx05, S44, x"fc93a039"); -- 52
      A(53) <= II (A(52), B(52), C(52), D(52), xx12, S41, x"655b59c3"); -- 53
      D(54) <= II (D(53), A(53), B(53), C(53), xx03, S42, x"8f0ccc92"); -- 54
      C(55) <= II (C(54), D(54), A(54), B(54), xx10, S43, x"ffeff47d"); -- 55
      B(56) <= II (B(55), C(55), D(55), A(55), xx01, S44, x"85845dd1"); -- 56
      A(57) <= II (A(56), B(56), C(56), D(56), xx08, S41, x"6fa87e4f"); -- 57
      D(58) <= II (D(57), A(57), B(57), C(57), xx15, S42, x"fe2ce6e0"); -- 58
      C(59) <= II (C(58), D(58), A(58), B(58), xx06, S43, x"a3014314"); -- 59
      B(60) <= II (B(59), C(59), D(59), A(59), xx13, S44, x"4e0811a1"); -- 60
      A(61) <= II (A(60), B(60), C(60), D(60), xx04, S41, x"f7537e82"); -- 61
      D(62) <= II (D(61), A(61), B(61), C(61), xx11, S42, x"bd3af235"); -- 62
      C(63) <= II (C(62), D(62), A(62), B(62), xx02, S43, x"2ad7d2bb"); -- 63
      B(64) <= II (B(63), C(63), D(63), A(63), xx09, S44, x"eb86d391"); -- 64

      Aout <= A(64) + iA;
      Bout <= B(64) + iB;
      Cout <= C(64) + iC;
      Dout <= D(64) + iD;
    end if;  
  end process;
end Behavioral;

