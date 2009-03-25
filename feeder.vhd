library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity feeder is
  port (next0 : in std_logic_vector (31 downto 0);
        next1 : in std_logic_vector (31 downto 0);
        next2 : in std_logic_vector (31 downto 0);

        hit : out std_logic;

        hitA : out std_logic_vector (31 downto 0);
        hitB : out std_logic_vector (31 downto 0);
        hitC : out std_logic_vector (31 downto 0);
        hitD : out std_logic_vector (31 downto 0);

        Clk : in std_logic);
end feeder;

architecture Behavioral of feeder is

  subtype nibble_t is std_logic_vector (3 downto 0);
  subtype byte_t is std_logic_vector (7 downto 0);
  subtype word_t is std_logic_vector (31 downto 0);

  component md5 is
    Port (x0 : in  word_t;
          x1 : in  word_t;
          x2 : in  word_t;
          x3 : in  word_t;
          x4 : in  word_t;
          x5 : in  word_t;
          A64 : out word_t;
          Aout : out word_t;
          Bout : out word_t;
          Cout : out word_t;
          Dout : out word_t;
          Clk : in std_logic);
  end component;

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

  function hexify16 (n : std_logic_vector (15 downto 0)) return word_t is
    variable result : word_t;
  begin
    result( 7 downto  0) := hexify (n ( 3 downto  0));
    result(15 downto  8) := hexify (n ( 7 downto  4));
    result(23 downto 16) := hexify (n (11 downto  8));
    result(31 downto 24) := hexify (n (15 downto 12));
  end hexify16;
  
  signal x0 : word_t;
  signal x1 : word_t;
  signal x2 : word_t;
  signal x3 : word_t;
  signal x4 : word_t;
  signal x5 : word_t;

  signal A64 : word_t;

  signal Aout : word_t;
  signal Bout : word_t;
  signal Cout : word_t;
  signal Dout : word_t;

  constant iA : word_t := x"67452301";
  constant iAneg : word_t := x"00000000" - iA;
begin

  m : md5 port map (x0=>x0,x1=>x1,x2=>x2,x3=>x3,x4=>x4,x5=>x5,
                    Aout=>Aout,Bout=>Bout,Cout=>Cout,Dout=>Dout,
                    Clk=>Clk,A64=>A64);

  hit <= '1' when A64(23 downto 0) = iAneg(23 downto 0) else '0';
  hitA <= Aout;
  hitB <= Bout;
  hitC <= Cout;
  hitD <= Dout;

  -- Feed through results to input.
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      x0( 7 downto  0) <= hexify (next0 ( 3 downto  0));
      x0(15 downto  8) <= hexify (next0 ( 7 downto  4));
      x0(23 downto 16) <= hexify (next0 (11 downto  8));
      x0(31 downto 24) <= hexify (next0 (15 downto 12));

      x1( 7 downto  0) <= hexify (next0 (19 downto 16));
      x1(15 downto  8) <= hexify (next0 (23 downto 20));
      x1(23 downto 16) <= hexify (next0 (27 downto 24));
      x1(31 downto 24) <= hexify (next0 (31 downto 28));

      x2( 7 downto  0) <= hexify (next1 ( 3 downto  0));
      x2(15 downto  8) <= hexify (next1 ( 7 downto  4));
      x2(23 downto 16) <= hexify (next1 (11 downto  8));
      x2(31 downto 24) <= hexify (next1 (15 downto 12));

      x3( 7 downto  0) <= hexify (next1 (19 downto 16));
      x3(15 downto  8) <= hexify (next1 (23 downto 20));
      x3(23 downto 16) <= hexify (next1 (27 downto 24));
      x3(31 downto 24) <= hexify (next1 (31 downto 28));

      x4( 7 downto  0) <= hexify (next2 ( 3 downto  0));
      x4(15 downto  8) <= x"80";
      x4(31 downto 16) <= x"0000";
      x5(31 downto  0) <= x"00000000";
--      x4(15 downto  8) <= hexify (next2 ( 7 downto  4));
--      x4(23 downto 16) <= hexify (next2 (11 downto  8));
--      x4(31 downto 24) <= hexify (next2 (15 downto 12));

--      x5( 7 downto  0) <= hexify (next2 (19 downto 16));
--      x5(15 downto  8) <= hexify (next2 (23 downto 20));
--      x5(23 downto 16) <= hexify (next2 (27 downto 24));
--      x5(31 downto 24) <= x"80";
      --x5(31 downto 24) <= hexify (next2 (31 downto 28));
    end if;
  end process;
end Behavioral;
