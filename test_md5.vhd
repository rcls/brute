
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
    Port (in0 : in  word_t;
          in1 : in  word_t;
          in2 : in  word_t;
          Aout : out word_t;
          Bout : out word_t;
          Cout : out word_t;
          Dout : out word_t;
          --bmon : out dataset_t (0 to 64);

          Clk : in std_logic);
   end component;
   signal in0 : word_t;
   signal in1 : word_t;
   signal in2 : word_t;
   signal Aout : word_t;
   signal Bout : word_t;
   signal Cout : word_t;
   signal Dout : word_t;
   signal bmon : dataset_t (0 to 64);

   signal Clk : std_logic;
begin
   UUT : md5 port map (in0=>in0,in1=>in1,in2=>in2,
	--bmon=>bmon,
   Aout=>Aout,Bout=>Bout,Cout=>Cout,Dout=>Dout,Clk=>Clk);

   process
   begin
      in0 <= x"00000000";
      in1 <= x"00000000";
      in2 <= x"00000000";
      Clk <= '0';
      -- First, clock 200 times so that outputs settle.
      for i in 1 to 200 loop
         Clk <= '0';
         wait for 0.5 us;
         Clk <= '1';
         wait for 0.5 us;
      end loop;
      -- Set some inputs and clock once.
      in0 <= x"e040a4f0";
      in1 <= x"7d4a91b5";
      in2 <= x"694f8475";
      Clk <= '0';
      wait for 0.5 us;
      Clk <= '1';
      wait for 0.5 us;

      -- Now clock in zeros 100 times again.
      in0 <= x"00000000";
      in1 <= x"00000000";
      in2 <= x"00000000";
      --x3 <= x"00000000";
      Clk <= '0';
      -- First, clock 100 times so that outputs settle.
      for i in 1 to 1000 loop
         Clk <= '0';
         wait for 0.5 us;
         Clk <= '1';
         wait for 0.5 us;
      end loop;
   end process;

end Behavioral;

