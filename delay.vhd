----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    07:27:10 03/28/2009 
-- Design Name: 
-- Module Name:    delay - Behavioral 
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

entity delay is
    generic (N : integer);
    Port ( D : in  STD_LOGIC_VECTOR (31 downto 0);
           Q : out  STD_LOGIC_VECTOR (31 downto 0);
           Clk : in std_logic);
end delay;

architecture Behavioral of delay is

  constant limit : integer := N - 3;
  
  signal count : integer range 0 to limit := 0;

  subtype word_t is std_logic_vector (31 downto 0);
  type mem_t is array (limit downto 0) of word_t;
  signal mem : mem_t;
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
