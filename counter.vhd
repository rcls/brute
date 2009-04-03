library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

library UNISIM;
use UNISIM.VComponents.all;

library work;
use work.defs.all;

-- We use 2 DSPs, one for the counter and one for the match comparison.
-- Hit is high the cycle after match==count.
entity counter is
  port (count : out word48_t;
        match : in word48_t;
        hit : out std_logic;
        clk : in std_logic);
end counter;

architecture Behavioral of counter is
  signal count_int : word48_t;
  signal sub_val : word48_t;
  signal match54 : std_logic_vector (53 downto 0);
  signal prev_sub_hi : std_logic;
begin
  -- The counter; we disable all inputs and force carry to 1.
  cnt : DSP48A
    generic map (
      A0REG => 1, -- Only C input in use.
      A1REG => 1,
      B0REG => 1,
      B1REG => 1,
      CARRYINREG => 1,
      CARRYINSEL => "OPMODE5",
      CREG => 0,
      DREG => 0,
      MREG => 1, -- Dummy, multiplier not in use.
      OPMODEREG => 0,
      PREG => 1, -- Register output.
      RSTTYPE => "SYNC") -- Not used.
    port map (
      BCOUT => open,
      CARRYOUT => open,
      P=> count,
      PCOUT=> count_int,

      A=> "00" & x"0000",
      B=> "00" & x"0000",
      C=> x"000000000000",
      CARRYIN=> '0',
      CEA=> '0',
      CEB=> '0',
      CEC=> '0',
      CECARRYIN=> '1',
      CED=> '0',
      CEM=> '0',
      CEOPMODE=> '0',
      CEP=> '1',
      CLK=> Clk,
      D=> "00" & x"0000",
      OPMODE=> "00111000", -- add,add / carry1,no-preadd / pcin,zero
      PCIN=> x"000000000000",
      RSTA=> '0',
      RSTB=> '0',
      RSTC=> '0',
      RSTCARRYIN=> '0',
      RSTD=> '0',
      RSTM=> '0',
      RSTP=> '0',
      RSTOPMODE=> '0');

  match54 <= "000000" & match;
  
  -- The hit subtractor.  PCIN comes from the counter, DAB comes from the match
  -- value.
  sub : DSP48A
    generic map (
      A0REG => 0,
      A1REG => 1,
      B0REG => 0,
      B1REG => 1,
      CARRYINREG => 1,
      CARRYINSEL => "OPMODE5",
      CREG => 1,
      DREG => 1,
      MREG => 1, -- Dummy, multiplier not in use.
      OPMODEREG => 0,
      PREG => 1, -- Register output.
      RSTTYPE => "SYNC") -- Not used.
    port map (
      BCOUT => open,
      CARRYOUT => open,
      P=> sub_val,
      PCOUT=> open,

      A=> match54 (35 downto 18),
      B=> match54 (17 downto 0),
      C=> x"000000000000",
      CARRYIN=> '0',
      CEA=> '1',
      CEB=> '1',
      CEC=> '0',
      CECARRYIN=> '0',
      CED=> '1',
      CEM=> '0',
      CEOPMODE=> '0',
      CEP=> '1',
      CLK=> Clk,
      D=> match54 (53 downto 36),
      OPMODE=> "10010111", -- sub,sub / carry0,no-preadd / pcin,dab
      PCIN=> count_int,
      RSTA=> '0',
      RSTB=> '0',
      RSTC=> '0',
      RSTCARRYIN=> '0',
      RSTD=> '0',
      RSTM=> '0',
      RSTP=> '0',
      RSTOPMODE=> '0');

  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      prev_sub_hi <= sub_val(47);
    end if;
  end process;

  hit <= not sub_val(47) and prev_sub_hi;
end Behavioral;
