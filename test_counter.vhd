
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

library work;
use work.defs.all;

entity test_counter is
end test_counter;

architecture Behavioral of test_counter is
  component counter is
    port (count : out word48_t;
          match : in word48_t;
          hit : out std_logic;
          clk : in std_logic);
  end component;
  signal count : word48_t;
  signal match : word48_t := x"000000000032";
  signal hit : std_logic;
  signal clk : std_logic;
begin
  UUT : counter port map (
    count=>count,match=>match,hit=>hit,clk=>clk);
  process
  begin
    for i in 1 to 100 loop
      Clk <= '0';
      wait for 0.5 us;
      Clk <= '1';
      wait for 0.5 us;
    end loop;
  end process;
end Behavioral;
