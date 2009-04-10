library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

library work;
use work.defs.all;

entity feeder is
  port (hit_read_addr : in byte_t;
        hit_ram_o : out word144_t;
        hit_idx : out byte_t;
        global_count : in word48_t;
        global_count_match : in std_logic;
        load_data : in word96_t;
        load_match : in std_logic;
        sample_match : in std_logic;
        Clk : in std_logic);
end feeder;

architecture Behavioral of feeder is

  signal md5_next : word96_t;           -- 3 input words to md5.
  signal md5_out : word128_t;           -- 4 output words from md5.

  -- The dual ported hit ram.
  type hit_ram_t is array (255 downto 0) of word144_t;
  signal hit_ram : hit_ram_t;
  -- The hit ram allocation counter.
  signal hit_idx_r : byte_t;
  -- Did we hit?
  signal hit : std_logic;

  component md5 is
    port (input : in word96_t;
          output : out word128_t;
          Clk : in std_logic);
  end component;

begin
  m : md5 port map (input => md5_next, output => md5_out, Clk => Clk);

  hit_idx <= hit_idx_r;

  process (Clk)
  begin
    if Clk'event and Clk = '1' then
      if hit = '1' then
        hit_ram (conv_integer(hit_idx_r)) <= global_count & md5_next;
        hit_idx_r <= hit_idx_r + 1;
      end if;

      hit_ram_o <= hit_ram (conv_integer (hit_read_addr));

      if md5_out(29 downto 0) = "00" & x"0000000" or sample_match = '1' then
        hit <= '1';
      else
        hit <= '0';
      end if;

      -- Calculate the next value to feed into the pipeline.
      if load_match = '1' then
        md5_next <= load_data;
      else
        md5_next <= md5_out (95 downto 0);
      end if;

    end if;
  end process;
end Behavioral;
