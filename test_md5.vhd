
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

entity test_md5 is
end test_md5;

architecture Behavioral of test_md5 is
  component md5 is
    Port (input : in word96_t;
          output : out word128_t;
			 bmon : out dataset_t (0 to 64);
          Clk : in std_logic);
   end component;
   signal input : word96_t;
   signal output : word128_t;
   signal bmon : dataset_t (0 to 64);

   signal Clk : std_logic;
begin
   UUT : md5 port map (input=>input,output=>output,bmon=>bmon,Clk=>Clk);

   process
   begin
      input <= x"000000000000000000000000";
      Clk <= '0';
      -- First, clock 200 times so that outputs settle.
      for i in 1 to 200 loop
         Clk <= '0';
         wait for 0.5 us;
         Clk <= '1';
         wait for 0.5 us;
      end loop;
      -- Set some inputs and clock once.
      input <= x"694f8475" & x"7d4a91b5" & x"e040a4f0";
      Clk <= '0';
      wait for 0.5 us;
      Clk <= '1';
      wait for 0.5 us;

      -- Now clock in zeros again.
      --input <= x"000000000000000000000000";

      for i in 1 to 1000 loop
         Clk <= '0';
         wait for 0.5 us;
         Clk <= '1';
         wait for 0.5 us;
      end loop;
   end process;

end Behavioral;

