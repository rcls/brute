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

  signal count : std_logic_vector (6 downto 0) := "0000000";

  constant limit : std_logic_vector (6 downto 0) := conv_std_logic_vector (N - 2, 7);

  subtype word_t is std_logic_vector (31 downto 0);
  type mem_t is array (127 downto 0) of word_t;
  signal mem : mem_t;
begin
  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      Q <= mem (conv_integer (count));
      mem (conv_integer (count)) <= D;

      if count = limit then
        count <= "0000000";
      else
        count <= count + 1;
      end if;
    end if;
  end process;
end Behavioral;
