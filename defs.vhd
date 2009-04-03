
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

package defs is
  subtype nibble_t is std_logic_vector (3 downto 0);
  subtype byte_t is std_logic_vector (7 downto 0);
  subtype word_t is std_logic_vector (31 downto 0);
  
  subtype word48_t is std_logic_vector (47 downto 0);

  type dataset_t is array (natural range <>) of word_t;

end defs;