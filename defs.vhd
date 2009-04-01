
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

package defs is
  subtype word is std_logic_vector (31 downto 0);
  
  type dataset is array (natural range <>) of word;

end defs;