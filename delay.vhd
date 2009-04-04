library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

---- Uncomment the following library declaration if instantiating
---- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

library work;
use work.defs.all;

entity delay is
    generic (N : integer);
    Port ( D : in  word_t;
           Q : out  word_t;
           Clk : in std_logic);
end delay;

architecture Behavioral of delay is

  constant limit : integer := N - 3;

  signal count : integer range 0 to limit := 0;

  signal mem : dataset_t (limit downto 0);
  signal reg : word_t;
  attribute ram_style : string;
  attribute ram_style of mem : signal is "block";
begin
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      reg <= mem (count);
      Q <= reg;
      mem (count) <= D;

      if count = limit then
        count <= 0;
      else
        count <= count + 1;
      end if;
    end if;
  end process;
end Behavioral;
